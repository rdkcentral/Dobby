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
 * This plugin uses all the hooks so set all the flags
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
    
    if (!mUtils->addMount(path, path, "bind", {"bind", "nodev","nosuid", "noexec" }))
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
    
    bool status;
    if(mUtils->exitStatus != 0)
        status = checkForOOM();
    
    AI_LOG_INFO("OOMCrash postHalt hook is running for container with hostname %s", mUtils->getContainerId().c_str());
    return status;
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
 *  @param[in]  val      the value of failcnt.
 *
 * @return true on success.
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
 * @brief Check for Out of Memory.
 *
 * @return false when OOM not detected.
 */

bool OOMCrash::checkForOOM()
{
    unsigned long failCnt;
    bool status;
    if (readCgroup(&failCnt) && (failCnt > 0))
    {
        AI_LOG_WARN("memory allocation failure detected in %s container, likely OOM (failcnt = %lu)", mUtils->getContainerId().c_str(), failCnt);
        status = createFileForOOM(); 
    }
    else
    {
        AI_LOG_WARN("No OOM failure detected in %s container", mUtils->getContainerId().c_str());
        status = false;
    }
    return status;
}

/**
 * @brief Create file if Out of Memory detected.
 *
 * @return true when file created.
 */

bool OOMCrash::createFileForOOM()
{
    char memoryExceedFile[150];
    const std::string path = mContainerConfig->rdk_plugins->oomcrash->data->path;
    if (mkdir(path.c_str(), 0755))
    {
        snprintf(memoryExceedFile,sizeof(memoryExceedFile), "%s/oom_crashed_%s_%s", path.c_str(), mUtils->getContainerId().c_str(), __TIME__);
        fp = fopen(memoryExceedFile,"w+");
        if (!fp)
        {
            if (errno != ENOENT)
                AI_LOG_ERROR("failed to open '%s' (%d - %s)", path.c_str(), errno, strerror(errno));
            return false;
        }
        AI_LOG_INFO("%s file created",memoryExceedFile);
        fclose(fp);
    }
    return true;
}
