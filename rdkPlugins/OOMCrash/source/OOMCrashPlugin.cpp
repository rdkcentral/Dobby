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
    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }

    const std::string path = mContainerConfig->rdk_plugins->oomcrash->data->path;
    if (!mUtils->mkdirRecursive(mRootfsPath + path.c_str(), 0755) && errno != EEXIST)
    {
        AI_LOG_ERROR("failed to create directory '%s' (%d - %s)", (mRootfsPath + path).c_str(), errno, strerror(errno));
        return false;
    }

    if (!mUtils->mkdirRecursive(path.c_str(), 0755) && errno != EEXIST)
    {
        AI_LOG_ERROR("failed to create directory '%s' (%d - %s)", path.c_str(), errno, strerror(errno));
        return false;
    }

    if (!mUtils->addMount(path, path, "bind", {"bind", "ro", "nodev","nosuid", "noexec" }))
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
    if (!mContainerConfig)
    {
        AI_LOG_WARN("Container config is null");
        return false;
    }
    
    bool oomDetected = false;
    if (mUtils->exitStatus != 0)
        oomDetected = checkForOOM();

    if (oomDetected == true)
        createFileForOOM();
    
    // Remove the crashFile if container exits normally or if no OOM detected
    if (mUtils->exitStatus == 0 || oomDetected == false)
    {
        struct stat buffer;
        std::string path = mContainerConfig->rdk_plugins->oomcrash->data->path;
        std::string crashFile = path + "/oom_crashed_" + mUtils->getContainerId() + ".txt";

        if (stat(crashFile.c_str(), &buffer) == 0)
        {
            remove(crashFile.c_str());
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
 * @brief Read cgroup file.
 *
 *  @param[out]  val      gives the number of times that the cgroup limit was exceeded.
 *
 * @return true on successfully reading from the file.
 */

bool OOMCrash::readCgroup(unsigned long *val)
{
    std::string path = "/sys/fs/cgroup/memory/" + mUtils->getContainerId() + "/memory.failcnt";

    FILE *fp = fopen(path.c_str(), "r");
    if (!fp)
    {
        if (errno != ENOENT)
            AI_LOG_ERROR("failed to open '%s' (%d - %s)", path.c_str(), errno, strerror(errno));

        return false;
    }

    char* line = nullptr;
    size_t len = 0;
    ssize_t rd;

    if ((rd = getline(&line, &len, fp)) < 0)
    {
        if (line)
            free(line);
        fclose(fp);
        AI_LOG_ERROR("failed to read cgroup file line (%d - %s)", errno, strerror(errno));
        return false;
    }

    *val = strtoul(line, nullptr, 0);

    fclose(fp);
    free(line);

    return true;
}

/**
 * @brief Check for Out of Memory by reading cgroup file.
 *
 * @return true if OOM detected.
 */

bool OOMCrash::checkForOOM()
{
    unsigned long failCnt;
    bool status;
    if (readCgroup(&failCnt) && (failCnt > 0))
    {
        AI_LOG_WARN("memory allocation failure detected in %s container, likely OOM (failcnt = %lu)", mUtils->getContainerId().c_str(), failCnt);
        status = true; 
    }
    else
    {
        AI_LOG_WARN("No OOM failure detected in %s container", mUtils->getContainerId().c_str());
        status = false;
    }
    return status;
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
        AI_LOG_INFO("%s file created",memoryExceedFile.c_str());
        fclose(fp);
    }
    else
    {
        if (errno == ENOENT)
            AI_LOG_ERROR("Path '%s' does not exist (%d - %s)", path.c_str(), errno, strerror(errno));
    }
}
