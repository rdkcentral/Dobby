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
#include "TimeZoneMonitor.h"

#include <Logging.h>

#include <unistd.h>
#include <errno.h>

REGISTER_RDK_PLUGIN(LocalTimePlugin);

LocalTimePlugin::LocalTimePlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                 const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 const std::string &rootfsPath)
    : mName("LocalTime"),
      mRootfsPath(rootfsPath),
      mContainerConfig(containerConfig),
      mUtils(utils)
{
    std::error_code ec;
    std::filesystem::path tzFilePath;
    if (containerConfig->rdk_plugins->localtime->data->path)
    {
        tzFilePath = containerConfig->rdk_plugins->localtime->data->path;
        if (tzFilePath.empty() || !std::filesystem::is_regular_file(tzFilePath, ec))
        {
            AI_LOG_ERROR("invalid timezone file path '%s', reverting to '/etc/localtime'", tzFilePath.c_str());
            tzFilePath = "/etc/localtime";
        }
    }
    else
    {
        tzFilePath = "/etc/localtime";
    }

    mTimeZoneMonitor = std::make_unique<TimeZoneMonitor>(std::move(tzFilePath));
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

    // the absolute path to the /etc/localtime file in the container rootfs
    const std::filesystem::path localTimePath = mRootfsPath + "/etc/localtime";
    const std::filesystem::path etcDirPath = localTimePath.parent_path();

    // create the /etc directory in the container rootfs if it doesn't already exist
    if (mUtils->mkdirRecursive(etcDirPath.string(), 0755) || (errno == EEXIST))
    {
        AI_LOG_INFO("set localtime path %s/localtime", etcDirPath.c_str());
    }
    else
    {
        AI_LOG_SYS_ERROR(errno, "failed to create dir @ %s", etcDirPath.c_str());
        return false;
    }

    // add the /etc/localtime file to the time zone monitor, it will create the
    // initial file in the container rootfs and update it whenever the real time
    // zone file changes on the host
    mTimeZoneMonitor->addPathToUpdate(localTimePath);

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
