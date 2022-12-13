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

    const int64_t devInputEventMajor = 13;
    const int64_t devInputEventMinor = 64;
    const int numDevices = 10;
    const std::string type{"c"};
    const std::string mode{"rw"};

    addDevices(devInputEventMajor, devInputEventMinor, numDevices, type, mode);

    mUtils->addMount("/dev/input/", "/dev/input/", "bind", {"bind", "nosuid", "noexec"});

    gid_t inputGroupId = getInputGroupId();

    addGidMapping(inputGroupId, inputGroupId);
    addAdditionalGid(inputGroupId);

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

// -----------------------------------------------------------------------------
/**
 *  @brief Adds devices to containe_config->linux->resources->devices
 *
 *  Multiple devices can be added. All devices will have major number
 *  equal to major param. Minor numbers are in range [minor .. minor + numDevices - 1].
 *
 *  @param[in]  major         major number of the devices that will be added
 *  @param[in]  minor         starting minor number of the devices
 *  @param[in]  numDevices    number of devices that will be added
 *  @param[in]  type          character ("c") of block ("b")
 *  @param[in]  mode          access mode
 */
void GamepadPlugin::addDevices(int64_t major, int64_t minor, int numDevices, const std::string& type, const std::string& mode) const
{
    rt_config_linux_resources* schemaResources = mContainerConfig->linux->resources;
    size_t old_devices_len = schemaResources->devices_len;

    schemaResources->devices_len += numDevices;
    schemaResources->devices = (rt_defs_linux_device_cgroup**)realloc(schemaResources->devices, sizeof(rt_defs_linux_device_cgroup*) * schemaResources->devices_len);

    rt_defs_linux_device_cgroup* device = schemaResources->devices[old_devices_len];

    for (int i = 0; i < numDevices; ++i)
    {
        device = (rt_defs_linux_device_cgroup*)calloc(1, sizeof(rt_defs_linux_device_cgroup));
        device->type = strdup(type.c_str());
        device->access = strdup(mode.c_str());
        device->major = major;
        device->major_present = true;
        device->minor = minor + i;
        device->minor_present = true;
        device->allow = true;
        device->allow_present = true;

        schemaResources->devices[old_devices_len + i] = device;
        device++;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds gid mapping  to container_config->linux->gid_mappings
 *
 *  @param[in]  host_id         host group id to be added to the mapping
 *  @param[in]  container_id    container group id to be added to the mapping
 */
void GamepadPlugin::addGidMapping(gid_t host_id, gid_t container_id) const
{
    rt_defs_id_mapping* newMapping = (rt_defs_id_mapping*)calloc(1, sizeof(rt_defs_id_mapping));
    newMapping->container_id = container_id;
    newMapping->container_id_present = true;
    newMapping->host_id = host_id;
    newMapping->host_id_present = true;
    newMapping->size = 1;
    newMapping->size_present = true;

    rt_config_linux* schemaLinux = mContainerConfig->linux;
    schemaLinux->gid_mappings_len++;
    schemaLinux->gid_mappings = (rt_defs_id_mapping**)realloc(schemaLinux->gid_mappings, sizeof(rt_defs_id_mapping*) * schemaLinux->gid_mappings_len);
    schemaLinux->gid_mappings[schemaLinux->gid_mappings_len - 1] = newMapping;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds additionalGid to container_config->process->user->additional_gids
 *
 *  @param[in]  gid   group id to be added
 */
void GamepadPlugin::addAdditionalGid(gid_t gid) const
{
    rt_dobby_schema_process_user* schemaUser = mContainerConfig->process->user;
    schemaUser->additional_gids_len++;
    schemaUser->additional_gids = (gid_t*)realloc(schemaUser->additional_gids, sizeof(gid_t) * schemaUser->additional_gids_len);
    schemaUser->additional_gids[schemaUser->additional_gids_len - 1] = gid;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Finds input group id in /etc/group file
 *
 *  Each line in /etc/group contains "group_name:password:group_id:group_list".
 *  Finds the line starting with 'input', then finds the group_id in that line.
 *
 *  @return input groups group id or -1 if group id is not found
 */
gid_t GamepadPlugin::getInputGroupId() const
{
    std::ifstream etcGroupFile;

    etcGroupFile.open("/etc/group");
    if (etcGroupFile.is_open())
    {
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
