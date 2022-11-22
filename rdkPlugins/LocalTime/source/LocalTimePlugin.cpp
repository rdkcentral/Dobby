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

#include "LocalTimePlugin.h"

#include <Logging.h>

#include <errno.h>
#include <unistd.h>
#include <limits.h>

REGISTER_RDK_PLUGIN(LocalTimePlugin);

LocalTimePlugin::LocalTimePlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                 const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 const std::string &rootfsPath)
    : mName("LocalTime"),
      mRootfsPath(rootfsPath),
      mContainerConfig(containerConfig),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

unsigned LocalTimePlugin::hookHints() const
{
    return IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
           IDobbyRdkPlugin::HintFlags::PreCreationFlag;
}

// -----------------------------------------------------------------------------
/**
 *  @brief postInstallation OCI hook.
 *
 *  All we need to do create symlink in the container rootfs to the real time
 *  zone file - matching the /etc/localtime entry outside the container.
 *
 *  @return true on success, false on failure.
 */
bool LocalTimePlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    // get the real path to the correct local time zone
    char pathBuf[PATH_MAX];
    ssize_t len = readlink("/etc/localtime", pathBuf, sizeof(pathBuf));
    if (len <= 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "readlink failed on '/etc/localtime'");
        return false;
    }

    const std::string localtimeInHost(pathBuf, len);
    const std::string localtimeInContainer = mRootfsPath + "/etc/localtime";

    if (localtimeInHost.empty())
    {
        AI_LOG_ERROR_EXIT("missing real timezone file path");
        return false;
    }
    else if (symlink(localtimeInHost.c_str(), localtimeInContainer.c_str()) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create /etc/localtime symlink");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief postInstallation OCI hook.
 *
 *  If set_tz parameter is set then its value should be a path to file.
 *  Read this file and put its contents into containers TZ env var.
 *
 *  @return true on success, false on failure.
 */
bool LocalTimePlugin::preCreation()
{
    AI_LOG_FN_ENTRY();

    if (!mContainerConfig)
    {
        AI_LOG_WARN("container config is null");
        return false;
    }

    const char* set_tz = mContainerConfig->rdk_plugins->localtime->data->set_tz;
    if (set_tz)
    {
        AI_LOG_DEBUG("set_tz is '%s'", set_tz);

        std::ifstream file(set_tz);
        if (file.is_open())
        {
            std::string tz;
            std::getline(file, tz);
            mUtils->addEnvironmentVar("TZ=" + tz);

            AI_LOG_DEBUG("read from set_tz: %s", tz.c_str());
        }
        else
        {
            AI_LOG_WARN("unable to open '%s'", set_tz);
            return false;
        }
    }
    else
    {
        AI_LOG_DEBUG("set_tz not set");
    }

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> LocalTimePlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_local_time* pluginConfig = mContainerConfig->rdk_plugins->localtime;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}
