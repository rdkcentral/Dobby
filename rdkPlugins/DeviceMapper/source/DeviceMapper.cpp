/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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

#include "DeviceMapper.h"

#include <Logging.h>
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <unistd.h>


REGISTER_RDK_PLUGIN(DeviceMapperPlugin);

DeviceMapperPlugin::DeviceMapperPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                       const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                       const std::string &rootfsPath)
    : mName("DeviceMapper"),
    mContainerConfig(containerConfig),
    mRootfsPath(rootfsPath),
    mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    if (!mContainerConfig || !mContainerConfig->rdk_plugins->devicemapper || !mContainerConfig->rdk_plugins->devicemapper->data)
    {
        mValid = false;
    }
    else
    {
        mValid = true;
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 */
unsigned DeviceMapperPlugin::hookHints() const
{
    return (IDobbyRdkPlugin::HintFlags::PreCreationFlag);
}

/**
 * @brief Dobby Hook - run in host namespace before container creation process
 */
bool DeviceMapperPlugin::preCreation()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_WARN("Invalid config file");
        return false;
    }

    if (mContainerConfig->rdk_plugins->devicemapper->data->devices_len == 0 || mContainerConfig->linux->devices_len == 0)
    {
        // No devices in plugin config or in container config, nothing to do
        return true;
    }

    // Get the major/minor IDs of the devices we're interested in
    std::vector<DevNode> deviceNodes;
    DevNode node;
    for (size_t i = 0; i < mContainerConfig->rdk_plugins->devicemapper->data->devices_len; i++)
    {
        if (getDevNodeFromPath(mContainerConfig->rdk_plugins->devicemapper->data->devices[i], node))
        {
            deviceNodes.emplace_back(node);
        }
    }

    // We know we have some devices we're interested in, but we didn't find any
    // of them, so something's gone wrong
    if (deviceNodes.size() == 0)
    {
        AI_LOG_FN_EXIT();
        return false;
    }

    // Fix up the devices list
    std::vector<DevNode> incorrectDevNodes;
    for (size_t i = 0; i < mContainerConfig->linux->devices_len; i++)
    {
        auto configDevice = mContainerConfig->linux->devices[i];
        std::string devicePath = configDevice->path;

        // Is this device one we're interested in?
        auto devIt = std::find_if(deviceNodes.begin(), deviceNodes.end(),
                                [&p = devicePath](DevNode n)
                                {
                                    return p == n.path;
                                });

        if (devIt == deviceNodes.end())
        {
            continue;
        }

        // Check if the ids in config match the real ids
        if (devIt->major == configDevice->major && devIt->minor == configDevice->minor)
        {
            // Config is correct, nothing to do
            AI_LOG_DEBUG("No fixup needed for %s", devIt->path.c_str());
            continue;
        }

        AI_LOG_INFO("Fixing major/minor ID for dev node '%s'", devIt->path.c_str());

        // Track that we changed this node, as we'll refer to it later to
        // fix up the cgroup allow list
        DevNode tmp = {
            devicePath,
            devIt->major,
            devIt->minor,
            configDevice->major,
            configDevice->minor,
            devIt->mode
        };
        incorrectDevNodes.emplace_back(tmp);

        // Edit the config
        configDevice->major = devIt->major;
        configDevice->minor = devIt->minor;
    }

    if (incorrectDevNodes.size() == 0)
    {
        // No more work to do
        AI_LOG_FN_EXIT();
        return true;
    }

    // Fix up the cgroup list
    for (size_t i = 0; i < mContainerConfig->linux->resources->devices_len; i++)
    {
        auto configDev = mContainerConfig->linux->resources->devices[i];

        // See if the device matches one we just fixed (have to compare ids as
        // the dev path isn't in this section)
        auto it = std::find_if(incorrectDevNodes.begin(), incorrectDevNodes.end(),
                                [&cd = configDev](DevNode node)
                                {
                                    return (cd->major == node.configMajor && cd->minor == node.configMinor);
                                });

        if (it == incorrectDevNodes.end())
        {
            continue;
        }

        AI_LOG_INFO("Fixing major/minor ID in cgroup allow list for dev node '%s'", it->path.c_str());
        configDev->major = it->major;
        configDev->minor = it->minor;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

/**
 * @brief Gets the actual details about the device node (major/minor ids)
 * at a given path
 *
 * @param[in]   path    The path of the device
 * @param[out]  node    The details about the device
 *
 * @return True if device node details got successfully
 */
bool DeviceMapperPlugin::getDevNodeFromPath(const std::string& path, DeviceMapperPlugin::DevNode& node)
{
    AI_LOG_FN_ENTRY();

    struct stat buf;
    if (stat(path.c_str(), &buf) != 0)
    {
        AI_LOG_SYS_WARN(errno, "failed to stat dev node @ '%s'", path.c_str());
        AI_LOG_FN_EXIT();
        return false;
    }

    node.path = path;
    node.major = (int64_t)major(buf.st_rdev);
    node.minor = (int64_t)minor(buf.st_rdev);
    node.mode = (buf.st_mode & 0666);

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
std::vector<std::string> DeviceMapperPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_device_mapper* pluginConfig = mContainerConfig->rdk_plugins->devicemapper;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods
