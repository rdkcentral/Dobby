/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "OOMCrashPlugin.h"

#include <map>

#define FIREBOLT_STATE          "fireboltState"
#define FIREBOLT_STATE_PREV     "fireboltState_prev"
/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(OOMCrash);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
OOMCrash::OOMCrash(std::shared_ptr<rt_dobby_schema> &containerConfig,
                             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::string &rootfsPath)
    : mName("OOMCrash"),
      mContainerConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 * This plugin uses the postInstallation and postHalt hooks, so set those flags
 */
unsigned OOMCrash::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
    IDobbyRdkPlugin::HintFlags::PostHaltFlag);
}

/**
 *  * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 *   */
bool OOMCrash::postInstallation()
{
    if (!mContainerConfig || !mContainerConfig->rdk_plugins || 
        !mContainerConfig->rdk_plugins->oomcrash || 
        !mContainerConfig->rdk_plugins->oomcrash->data)
    {
        AI_LOG_WARN("Container config or plugin data is null");
        return false;
    }

    const char *pathPtr = mContainerConfig->rdk_plugins->oomcrash->data->path;
    const std::string path = pathPtr ? pathPtr : "";
    if (path.empty())
    {
        AI_LOG_INFO("OOMCrash path not configured, skipping mount setup for container '%s'", mUtils->getContainerId().c_str());
        return true;
    }

    if (!mUtils->mkdirRecursive((mRootfsPath + path).c_str(), 0755) && errno != EEXIST)
    {
        AI_LOG_ERROR("failed to create directory '%s' (%d - %s)", (mRootfsPath + path).c_str(), errno, strerror(errno));
        return false;
    }

    if (!mUtils->mkdirRecursive(path.c_str(), 0755) && errno != EEXIST)
    {
        AI_LOG_ERROR("failed to create directory '%s' (%d - %s)", path.c_str(), errno, strerror(errno));
        return false;
    }

    if (!mUtils->addMount(path, path, "bind", {"bind", "ro", "nodev", "nosuid", "noexec"}))
    {
        AI_LOG_WARN("failed to add mount %s", path.c_str());
        return false;
    }

    AI_LOG_INFO("OOMCrash postInstallation hook is running for container with hostname %s", mUtils->getContainerId().c_str());
    return true;
}

/**
 * @brief Dobby Hook - Run in host namespace when container terminates
 */
bool OOMCrash::postHalt()
{
    if (!mContainerConfig || !mContainerConfig->rdk_plugins || 
        !mContainerConfig->rdk_plugins->oomcrash || 
        !mContainerConfig->rdk_plugins->oomcrash->data)
    {
        AI_LOG_WARN("Container config or plugin data is null");
        return false;
    }

    bool oomDetected = checkForOOM();

    const char *pathPtr = mContainerConfig->rdk_plugins->oomcrash->data->path;
    const std::string path = pathPtr ? pathPtr : "";

    if (oomDetected && !path.empty())
        createFileForOOM();

    // Remove the crashFile if container exits normally or if no OOM detected
    if (!path.empty() && (mUtils->exitStatus == 0 || !oomDetected))
    {
        std::string crashFile = path + "/oom_crashed_" + mUtils->getContainerId() + ".txt";
        if (remove(crashFile.c_str()) != 0)
        {
            if (errno != ENOENT)
            {
                perror("Failed to remove crash file");
                AI_LOG_WARN("Could not remove crash file: %s (%d - %s)", crashFile.c_str(), errno, strerror(errno));
            }
        }
        else
        {
            AI_LOG_INFO("%s file removed", crashFile.c_str());
        }
    }

    AI_LOG_INFO("OOMCrash postHalt hook is running for container with hostname %s", mUtils->getContainerId().c_str());
    return true;
}

// End hook methods

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */

std::vector<std::string> OOMCrash::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_oom_crash* pluginConfig = mContainerConfig->rdk_plugins->oomcrash;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

/**
 * @brief Read the oom_kill counter from the cgroup memory.oom_control file.
 *
 *  The memory.oom_control file contains multiple key-value lines, e.g.:
 *
 *  Kernel >= 4.13:
 *    oom_kill_disable 0
 *    under_oom        0
 *    oom_kill         1
 *
 *  Kernel < 4.13:
 *    oom_kill_disable 0
 *    under_oom        0
 *
 *  On older kernels the 'oom_kill' counter does not exist, so we fall back
 *  to the 'under_oom' flag which is 1 while the cgroup is in OOM state.
 *
 *  @param[out]  val   Set to the value of the 'oom_kill' field (or 'under_oom'
 *                     on older kernels) on success.
 *
 * @return true on successfully reading and parsing the field.
 */

bool OOMCrash::readCgroup(unsigned long *val)
{
    std::string path = "/sys/fs/cgroup/memory/" + mUtils->getContainerId() + "/memory.oom_control";

    FILE *fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        AI_LOG_ERROR("failed to open '%s' (%d - %s)", path.c_str(), errno, strerror(errno));
        return false;
    }

    char* line = nullptr;
    size_t len = 0;
    ssize_t rd;
    bool foundOomKill = false;
    unsigned long underOom = 0;
    bool foundUnderOom = false;

    while ((rd = getline(&line, &len, fp)) > 0)
    {
        unsigned long v;
        // sscanf won't match "oom_kill_disable" because the space in the
        // format requires whitespace where "_disable" has an underscore.
        if (sscanf(line, "oom_kill %lu", &v) == 1)
        {
            *val = v;
            foundOomKill = true;
            break;
        }
        if (sscanf(line, "under_oom %lu", &v) == 1)
        {
            underOom = v;
            foundUnderOom = true;
        }
    }

    if (line)
        free(line);
    fclose(fp);

    // Prefer oom_kill (kernel >= 4.13); fall back to under_oom for older kernels
    if (foundOomKill)
        return true;

    if (foundUnderOom)
    {
        AI_LOG_INFO("'oom_kill' field not present (kernel < 4.13), using 'under_oom' fallback");
        *val = underOom;
        return true;
    }

    AI_LOG_ERROR("neither 'oom_kill' nor 'under_oom' found in '%s'", path.c_str());
    return false;
}

/**
 * @brief Check if memory (or memory+swap) max usage reached the configured
 *        limit, indicating the container hit its memory ceiling.
 *
 *  This is used as a fallback OOM indicator on older kernels (< 4.13) where
 *  the oom_kill counter does not exist and under_oom is transient.
 *  memory.max_usage_in_bytes is the high-water mark and persists until the
 *  cgroup is destroyed.
 *
 * @return true if max usage >= limit for memory or memory+swap.
 */
bool OOMCrash::isMemoryAtLimit()
{
    std::string basePath = "/sys/fs/cgroup/memory/" + mUtils->getContainerId();

    const char *pairs[][2] = {
        { "/memory.max_usage_in_bytes",      "/memory.limit_in_bytes" },
        { "/memory.memsw.max_usage_in_bytes", "/memory.memsw.limit_in_bytes" },
    };

    for (const auto &pair : pairs)
    {
        unsigned long maxUsage = 0, limit = 0;
        std::string maxPath  = basePath + pair[0];
        std::string limPath  = basePath + pair[1];

        FILE *fpMax = fopen(maxPath.c_str(), "r");
        FILE *fpLim = fopen(limPath.c_str(), "r");

        bool ok = (fpMax && fpLim &&
                   fscanf(fpMax, "%lu", &maxUsage) == 1 &&
                   fscanf(fpLim, "%lu", &limit) == 1);

        if (fpMax) fclose(fpMax);
        if (fpLim) fclose(fpLim);

        if (ok && limit > 0 && maxUsage >= limit)
        {
            AI_LOG_INFO("%s=%lu reached %s=%lu", pair[0]+1, maxUsage, pair[1]+1, limit);
            return true;
        }
    }

    return false;
}

/**
 * @brief Check for Out of Memory by reading cgroup files.
 *
 *  Detection priority:
 *    1. oom_kill  > 0            (kernel >= 4.13, definitive)
 *    2. under_oom > 0            (kernel <  4.13, transient flag)
 *    3. max_usage_in_bytes >= limit  (all kernels, persistent high-water mark)
 *
 * @return true if OOM detected.
 */

bool OOMCrash::checkForOOM()
{
    unsigned long oomKill = 0;
    bool cgroupRead = readCgroup(&oomKill);

    // Priority 1 & 2: oom_kill or under_oom confirmed OOM
    if (cgroupRead && oomKill > 0)
    {
        AI_LOG_INFO("oom_control reports OOM (value=%lu) for container '%s'",
                    oomKill, mUtils->getContainerId().c_str());
    }
    // Priority 3: on kernel < 4.13 under_oom may have cleared — check max_usage
    else if (isMemoryAtLimit())
    {
        AI_LOG_WARN("oom_control did not confirm OOM but max memory usage reached limit "
                    "for container '%s'", mUtils->getContainerId().c_str());
    }
    else
    {
        AI_LOG_INFO("No OOM kill detected in container '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    // OOM kill confirmed - retrieve firebolt state from annotations.
    // AppService often transitions the app to "background" after the OOM kill
    // but before postHalt runs.  Since the container exited abnormally, prefer
    // the previous fireboltState value (which was the state at the time of the
    // actual OOM kill) over the current value which may have been overwritten
    // by a post-crash transition.
    std::map<std::string, std::string> annotations = mUtils->getAnnotations();
    std::string fireboltState;

    auto prevIt = annotations.find(FIREBOLT_STATE_PREV);
    if (prevIt != annotations.end())
    {
        fireboltState = prevIt->second;
        AI_LOG_INFO("Using previous fireboltState '%s' (current may have been "
                    "set after OOM kill)", fireboltState.c_str());
    }
    else
    {
        auto it = annotations.find(FIREBOLT_STATE);
        if (it != annotations.end())
        {
            fireboltState = it->second;
        }
    }

    if (!fireboltState.empty())
    {
        AI_LOG_WARN("OOM kill detected: container '%s' fireboltState '%s'",
                    mUtils->getContainerId().c_str(), fireboltState.c_str());
    }
    else
    {
        AI_LOG_WARN("OOM kill detected: container '%s' (firebolt state unknown)",
                    mUtils->getContainerId().c_str());
    }

    return true;
}

/**
 * @brief Create OOM crash file named oom_crashed_<container_name>.txt on the configured path.
 *
 */

void OOMCrash::createFileForOOM()
{
    std::string memoryExceedFile;
    std::string path = mContainerConfig->rdk_plugins->oomcrash->data->path;
    
    struct stat buffer;
    if (stat(path.c_str(), &buffer)==0)
    {
        memoryExceedFile = path + "/oom_crashed_" + mUtils->getContainerId() + ".txt";
        FILE *fp = fopen(memoryExceedFile.c_str(), "w+");
        if (!fp)
        {
            if (errno != ENOENT)
                AI_LOG_ERROR("failed to open '%s' (%d - %s)", path.c_str(), errno, strerror(errno));
        }
        else
        {
            AI_LOG_INFO("%s file created",memoryExceedFile.c_str());
            fclose(fp);
        }
    }
    else
    {
        if (errno == ENOENT)
            AI_LOG_ERROR("Path '%s' does not exist (%d - %s)", path.c_str(), errno, strerror(errno));
    }
}
