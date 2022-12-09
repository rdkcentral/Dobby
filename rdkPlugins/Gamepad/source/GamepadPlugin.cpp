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

#include "GamepadPlugin.h"

// #include <linux/limits.h>
// #include <sys/mount.h>
// #include <sys/stat.h>
// #include <sys/types.h>
// #include <fcntl.h>
// #include <unistd.h>
// #include <mntent.h>
#include <iostream>
#include <sstream>

REGISTER_RDK_PLUGIN(GamepadPlugin);

GamepadPlugin::GamepadPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                     const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                     const std::string &rootfsPath)
    : mName("Gamepad"),
      mContainerConfig(containerConfig),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

unsigned GamepadPlugin::hookHints() const
{
    return (IDobbyRdkPlugin::HintFlags::PostInstallationFlag);
}

// Begin Hook Methods

/**
 * @brief Dobby Hook - run in host namespace *once* when container bundle is downloaded
 */
bool GamepadPlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    // 1. add devices
    rt_config_linux_resources* schemaResources = mContainerConfig->linux->resources;
    size_t old_devices_len = schemaResources->devices_len;
    const int FIRST_CONTROLLER = 64;
    const int NUM_CONTROLLERS = 10;

    schemaResources->devices_len += NUM_CONTROLLERS;
    schemaResources->devices = (rt_defs_linux_device_cgroup**)realloc(schemaResources->devices, sizeof(rt_defs_linux_device_cgroup*) * schemaResources->devices_len);

    rt_defs_linux_device_cgroup* device = schemaResources->devices[old_devices_len];

    for (int i = 0; i < NUM_CONTROLLERS; ++i)
    {
        device = (rt_defs_linux_device_cgroup*)calloc(1, sizeof(rt_defs_linux_device_cgroup));
        device->type = strdup("c");
        device->access = strdup("rw");
        device->major = 13;
        device->major_present = true;
        device->minor = FIRST_CONTROLLER + i;
        device->minor_present = true;
        device->allow = true;
        device->allow_present = true;

        schemaResources->devices[old_devices_len + i] = device;
        device++;
    }


    // 2. add /dev/input mount, "nodev"(MS_NODEV) flag removed
    mUtils->addMount("/dev/input/", "/dev/input/", "bind", {"bind", "nosuid", "noexec"});


    // 3. add input group to gidMapping
    int inputGroupId = getInputGroupId();
    //printf("inputGroupId: %d\n", inputGroupId);
    rt_defs_id_mapping* newMapping = (rt_defs_id_mapping*)calloc(1, sizeof(rt_defs_id_mapping));
    newMapping->container_id = inputGroupId;
    newMapping->container_id_present = true;
    newMapping->host_id = inputGroupId;
    newMapping->host_id_present = true;
    newMapping->size = 1;
    newMapping->size_present = true;

    rt_config_linux* schemaLinux = mContainerConfig->linux;
    schemaLinux->gid_mappings_len++;
    schemaLinux->gid_mappings = (rt_defs_id_mapping**)realloc(schemaLinux->gid_mappings, sizeof(rt_defs_id_mapping*) * schemaLinux->gid_mappings_len);
    schemaLinux->gid_mappings[schemaLinux->gid_mappings_len - 1] = newMapping;


    // 4. add additionalGids to process->user
    rt_dobby_schema_process_user* schemaUser = mContainerConfig->process->user;
    schemaUser->additional_gids_len++;
    schemaUser->additional_gids = (gid_t*)realloc(schemaUser->additional_gids, sizeof(gid_t) * schemaUser->additional_gids_len);
    schemaUser->additional_gids[schemaUser->additional_gids_len - 1] = inputGroupId;

    AI_LOG_FN_EXIT();
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
std::vector<std::string> GamepadPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_gamepad* pluginConfig = mContainerConfig->rdk_plugins->gamepad;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

// Begin private methods

int GamepadPlugin::getInputGroupId() const
{
    std::ifstream etcGroupFile;

    etcGroupFile.open("/etc/group");
    if (etcGroupFile.is_open())
    {
        // each line in /etc/group contains "group_name:password:group_id:group_list"
        // find the line starting with 'input', then find the group_id in that line

        std::string line;
        while (std::getline(etcGroupFile, line))
        {
            if (line.rfind("input", 0) != std::string::npos)
            {
                std::stringstream ss (line);
                std::string str;
                for (int i = 0; i < 3; ++i)
                {
                    if (!std::getline(ss, str, ':'))
                    {
                        return -1;
                    }
                }

                return std::stoi(str);
            }
        }
    }

    return -1;
}

// End private methods
