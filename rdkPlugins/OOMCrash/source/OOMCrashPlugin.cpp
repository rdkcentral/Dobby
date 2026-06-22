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

    bool oomDetected = false;
    if (mUtils->exitStatus != 0)
        oomDetected = checkForOOM();

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
 * @brief Read the cgroup OOM kill counter for the container.
 *
 *  cgroups v1: parses memory.oom_control from
 *    /sys/fs/cgroup/memory/<id>/memory.oom_control
 *
 *    Kernel >= 4.13 exposes an 'oom_kill' field — a monotonic count of
 *    processes killed by the OOM killer.  This is the preferred value.
 *
 *    Kernel < 4.13 does not have 'oom_kill'; 'under_oom' is 1 while the
 *    cgroup is actively under OOM pressure (transient — may clear before
 *    postHalt runs; isMemoryAtLimit() is then used as a final fallback).
 *
 *    Note: memory.failcnt counts allocation failures, NOT OOM kills.
 *    With swap available the kernel may swap rather than kill, and the
 *    counter resets to 0 on cgroup recreation, making it unreliable.
 *
 *  cgroups v2: reads the 'oom_kill' field from
 *    /sys/fs/cgroup/<id>/memory.events — a monotonic count of OOM kills.
 *    Falls back to the system.slice scope path if the direct path is absent.
 *
 *  @param[out]  val   Set to the oom_kill counter (v1 kernel >= 4.13 or v2),
 *                     or the under_oom flag (v1 kernel < 4.13) on success.
 *
 * @return true on successfully reading and parsing the value.
 */

bool OOMCrash::readCgroup(unsigned long *val)
{
    const std::string containerId = mUtils->getContainerId();

    // Detect cgroups version by checking for the v2 unified hierarchy
    struct stat st;
    const bool isCgroupV2 = (stat("/sys/fs/cgroup/cgroup.controllers", &st) == 0);

    std::string path;
    if (isCgroupV2)
    {
        // On cgroups v2, OOM events are in memory.events under the container's
        // cgroup directory within the unified hierarchy
        path = "/sys/fs/cgroup/" + containerId + "/memory.events";
    }
    else
    {
        // On cgroups v1, use the legacy per-controller path
        path = "/sys/fs/cgroup/memory/" + containerId + "/memory.oom_control";
    }

    FILE *fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        // On v2 the container may be scoped under system.slice
        if (isCgroupV2 && errno == ENOENT)
        {
            path = "/sys/fs/cgroup/system.slice/dobby-" + containerId + ".scope/memory.events";
            fp = fopen(path.c_str(), "r");
        }

        if (!fp)
        {
            if (errno != ENOENT)
                AI_LOG_ERROR("failed to open '%s' (%d - %s)", path.c_str(), errno, strerror(errno));

            return false;
        }
    }

    char* line = nullptr;
    size_t len = 0;
    ssize_t rd;

    if (isCgroupV2)
    {
        // memory.events is a key-value file, look for "oom_kill <N>"
        bool found = false;
        *val = 0;
        while ((rd = getline(&line, &len, fp)) >= 0)
        {
            unsigned long v = 0;
            if (sscanf(line, "oom_kill %lu", &v) == 1)
            {
                *val = v;
                found = true;
                break;
            }
        }
        free(line);
        fclose(fp);
        if (!found)
        {
            AI_LOG_ERROR("'oom_kill' key not found in '%s'", path.c_str());
            return false;
        }
        return true;
    }
    else
    {
        // v1: parse key-value memory.oom_control.
        // Prefer 'oom_kill' (kernel >= 4.13, monotonic); fall back to
        // 'under_oom' (kernel < 4.13, transient — 1 while OOM is active).
        unsigned long oomKill = 0, underOom = 0;
        bool foundOomKill = false, foundUnderOom = false;

        while ((rd = getline(&line, &len, fp)) > 0)
        {
            unsigned long v;
            if (sscanf(line, "oom_kill %lu", &v) == 1)
            {
                oomKill = v;
                foundOomKill = true;
            }
            else if (sscanf(line, "under_oom %lu", &v) == 1)
            {
                underOom = v;
                foundUnderOom = true;
            }
        }
        free(line);
        fclose(fp);

        if (foundOomKill)
        {
            *val = oomKill;
            return true;
        }
        if (foundUnderOom)
        {
            AI_LOG_INFO("'oom_kill' not present (kernel < 4.13), using 'under_oom' value");
            *val = underOom;
            return true;
        }
        AI_LOG_ERROR("neither 'oom_kill' nor 'under_oom' found in '%s'", path.c_str());
        return false;
    }
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
 *    1. oom_kill > 0   — from memory.oom_control (v1) or memory.events (v2).
 *                        Authoritative: if readCgroup() succeeds and returns 0,
 *                        the kernel confirms no OOM kill occurred and detection
 *                        stops here (isMemoryAtLimit() is NOT consulted).
 *    2. isMemoryAtLimit() — only reached when readCgroup() itself failed (cgroup
 *                        file unreadable).  Compares max usage against the
 *                        configured limit as a last-resort heuristic.
 *
 * @return true if OOM detected.
 */

bool OOMCrash::checkForOOM()
{
    unsigned long oomKill = 0;
    bool cgroupRead = readCgroup(&oomKill);

    if (cgroupRead && oomKill > 0)
    {
        // cgroup counter is authoritative — OOM kill confirmed
        AI_LOG_INFO("oom_control reports OOM (value=%lu) for container '%s'",
                    oomKill, mUtils->getContainerId().c_str());
    }
    else if (cgroupRead)
    {
        // cgroup read succeeded and counter is 0 — kernel says no OOM kill
        AI_LOG_INFO("No OOM kill detected in container '%s'", mUtils->getContainerId().c_str());
        return false;
    }
    else if (isMemoryAtLimit())
    {
        // cgroup file was unreadable — fall back to high-water-mark heuristic
        AI_LOG_WARN("cgroup unreadable; max memory usage reached limit for container '%s'",
                    mUtils->getContainerId().c_str());
    }
    else
    {
        AI_LOG_INFO("No OOM kill detected in container '%s'", mUtils->getContainerId().c_str());
        return false;
    }

    // OOM kill confirmed - retrieve firebolt state from annotations.
    // Both the previous (fireboltState_prev) and current (fireboltState) values
    // are read.  AppService often transitions the app (e.g. to "background")
    // after the OOM kill but before postHalt runs, so the current value may
    // reflect a post-crash transition rather than the state at the time of the
    // kill.  fireboltState_prev is therefore preferred for the final log; both
    // are reported so the transition can be observed in the logs.
    std::map<std::string, std::string> annotations = mUtils->getAnnotations();
    std::string fireboltState;

    auto prevIt = annotations.find(FIREBOLT_STATE_PREV);
    if (prevIt != annotations.end())
    {
        fireboltState = prevIt->second;
        std::string currentState;
        auto curIt = annotations.find(FIREBOLT_STATE);
        if (curIt != annotations.end())
            currentState = curIt->second;
        AI_LOG_INFO("Using previous fireboltState '%s' (current '%s' may have been "
                    "set after OOM kill)", fireboltState.c_str(), currentState.c_str());
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
