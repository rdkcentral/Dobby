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
#include <iomanip>

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

    // creates file descriptor to a volatile file that lives in RAM
    int hostFd = AnonymousFile().create();
    if (hostFd == -1)
    {
        AI_LOG_ERROR_EXIT("failed to create anonymous file in a host namespace");
        return false;
    }

    // duplicates file descriptor, as nearly 3 as possible, which will be carried to a container namespace
    int containerFd = mUtils->addFileDescriptor(mName, hostFd);
    close(hostFd);
    if (containerFd == -1)
    {
        AI_LOG_ERROR_EXIT("failed to add file descriptor %d to preserve container list", hostFd);
        return false;
    }

    std::ostringstream envVar;
    envVar << "BREAKPAD_FD=" << containerFd;

    // creates environment variable for breakpad-wrapper library purposes
    if (!mUtils->addEnvironmentVar(envVar.str()))
    {
        AI_LOG_ERROR_EXIT("failed to add BREAKPAD_FD environment variable with value %d", containerFd);
        return false;
    }

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
#define FIREBOLT_STATE  "fireboltState"
#define MINIDUMP_FILENAME_LENGTH 44
#define MINIDUMP_FN_SEPERATOR "<#=#>"

std::string Minidump::getDestinationFile()
{
    // If an app crashes multiple times, a previous dump might still exist in the destination
    // path. Append the current date/time to the filename to prevent conflicts
    auto now = std::chrono::system_clock::now();
    std::time_t currentTime = std::chrono::system_clock::to_time_t(now);
    std::stringstream timeString;
    timeString << std::put_time(std::localtime(&currentTime), "%FT%T");
    std::string destFile;
    std::string fileName;
    std::string containerId = mUtils->getContainerId();

    std::string dir(mContainerConfig->rdk_plugins->minidump->data->destination_path);

    std::map<std::string, std::string> annotations = mUtils->getAnnotations();

    auto it = annotations.find(FIREBOLT_STATE);
    if (it != annotations.end()) {
        // Format: containerId<#=#>state<#=#>timestamp
        // Timestamp length is fixed at 19 chars (e.g., "2021-07-06T12:34:56")
        // Separator length is 5 chars each
        // Total fixed: 19 (timestamp) + 5 (sep1) + 5 (sep2) = 29 chars
        // Available for containerId + state: 44 - 29 = 15 chars
        
        std::string state = it->second.c_str();
        int fixedLength = timeString.str().length() + (2 * MINIDUMP_FN_SEPERATOR.length()); // timestamp + 2 separators
        int availableLength = MINIDUMP_FILENAME_LENGTH - fixedLength;
        
        // Allocate space: try to keep full state, truncate containerId if needed
        int containerIdLength = availableLength - state.length();
        if (containerIdLength < 1)
        {
            // If state is too long, truncate state and give containerId minimal space
            containerIdLength = 1;
            state.resize(availableLength - containerIdLength);
        }
        else if (containerIdLength > (int)containerId.length())
        {
            // If full containerId fits, use it all
            containerIdLength = containerId.length();
        }
        
        std::string truncatedContainerId = containerId.substr(0, containerIdLength);
        fileName = truncatedContainerId + MINIDUMP_FN_SEPERATOR + state + MINIDUMP_FN_SEPERATOR + timeString.str();
        destFile = dir + "/" + fileName + ".dmp";
        AI_LOG_INFO("Firebolt state: %s, minidump filename: %s", it->second.c_str(), destFile.c_str());
    }else{
        // Format: containerId<#=#>timestamp
        // Timestamp length is fixed at 19 chars
        // Separator length is 5 chars
        // Total fixed: 19 (timestamp) + 5 (separator) = 24 chars
        // Available for containerId: 44 - 24 = 20 chars
        
        int fixedLength = timeString.str().length() + MINIDUMP_FN_SEPERATOR.length(); // timestamp + separator
        int containerIdLength = MINIDUMP_FILENAME_LENGTH - fixedLength;
        
        if (containerIdLength > (int)containerId.length())
        {
            containerIdLength = containerId.length();
        }
        else if (containerIdLength < 1)
        {
            containerIdLength = 1;
        }
        
        std::string truncatedContainerId = containerId.substr(0, containerIdLength);
        fileName = truncatedContainerId + MINIDUMP_FN_SEPERATOR + timeString.str();
        destFile = dir + "/" + fileName + ".dmp";
        AI_LOG_INFO("Firebolt state not found");
    }

    return destFile;
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

