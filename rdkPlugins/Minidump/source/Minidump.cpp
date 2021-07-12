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

#include "Minidump.h"
#include "AnonymousFile.h"

#include <sstream>
#include <unistd.h>

/**
 * Need to do this at the start of every plugin to make sure the correct
 * C methods are visible to allow PluginLauncher to find the plugin
 */
REGISTER_RDK_PLUGIN(Minidump);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
Minidump::Minidump(std::shared_ptr<rt_dobby_schema> &containerConfig,
                   const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                   const std::string &rootfsPath)
    : mName("Minidump")
    , mContainerConfig(containerConfig)
    , mRootfsPath(rootfsPath)
    , mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 */
unsigned Minidump::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PreCreationFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag
    );
}

/**
 * @brief OCI Hook - Run in host namespace
 */
bool Minidump::preCreation()
{
    AI_LOG_FN_ENTRY();

    size_t fileSize = 10 * 1024 * 1024; // default value
    if (mContainerConfig->rdk_plugins->minidump->data->size_present)
    {
        fileSize = mContainerConfig->rdk_plugins->minidump->data->size;
    }

    // creates file descriptor to a volatile file bounded by size that lives in RAM
    int hostFd = AnonymousFile(fileSize).create();
    if (hostFd == -1)
    {
        AI_LOG_ERROR_EXIT("failed to create anonymous file in a host namespace");
        return false;
    }

    // duplicates file descriptor, as nearly 3 as possible, which will be carried to a container namespace
    int containerFd = mUtils->addFileDescriptor(mName, hostFd);
    if (containerFd == -1)
    {
        AI_LOG_ERROR_EXIT("failed to add file descriptor %d to preserve container list", hostFd);
        close(hostFd);
        return false;
    }

    std::ostringstream envVar;
    envVar << "BREAKPAD_FD=" << containerFd;

    // creates environment variable for breakpad-wrapper library purposes
    if (!mUtils->addEnvironmentVar(envVar.str()))
    {
        AI_LOG_ERROR_EXIT("failed to add BREAKPAD_FD environment variable with value %d", containerFd);
        close(hostFd);
        return false;
    }

    close(hostFd);

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Dobby Hook - Run in host namespace when container terminates
 */
bool Minidump::postHalt()
{
    AI_LOG_FN_ENTRY();

    // gets file descriptor established at preCreateHook
    auto fileFds = mUtils->files(mName);
    const auto count = fileFds.size();
    if (count != 1)
    {
        AI_LOG_ERROR_EXIT("Incorrect number of fds passed to container namespace: %zu", count);
        return false;
    }

    int hostFd = fileFds.front();
    std::string destFile = getDestinationFile();

    // copies content of volatile file from RAM to a disk
    bool success = AnonymousFile(hostFd).copyContentTo(destFile);
    if (success)
    {
        AI_LOG_INFO("Minidump copied to: %s", destFile.c_str());
    }

    close(hostFd);

    AI_LOG_FN_EXIT();
    return success;
}

/**
 * @brief Creates a target location for the file where minidumps will be loaded
 *
 * It consist of:
 *   - destination path (/opt/minidumps or /opt/secure/minidumps) from config.json
 *   - container id (e.g. "de.sky.ZDF")
 *   - extension *.dmp
 *
 * @return Destination minidump file path string
 */
std::string Minidump::getDestinationFile()
{
    return std::string(mContainerConfig->rdk_plugins->minidump->data->destination_path)
        + "/" + mUtils->getContainerId() + ".dmp";
}

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> Minidump::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_minidump* pluginConfig = mContainerConfig->rdk_plugins->minidump;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}
