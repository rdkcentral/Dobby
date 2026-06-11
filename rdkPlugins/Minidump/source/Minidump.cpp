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

#include <chrono>
#include <sstream>
#include <unistd.h>
#include <iomanip>
#include <sys/stat.h>

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
    std::string destFile = getDestinationFile(hostFd);

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
#define FIREBOLT_STATE          "fireboltState"
#define FIREBOLT_STATE_TS       "fireboltState_ts"
#define FIREBOLT_STATE_PREV     "fireboltState_prev"
#define FIREBOLT_STATE_PREV_TS  "fireboltState_prev_ts"
#define MINIDUMP_FILENAME_LENGTH 44
#define MINIDUMP_FN_SEPERATOR "<#=#>"

// Helper to safely parse a millisecond timestamp string, returns 0 on failure
static long long parseTimestampMs(const std::string &str)
{
    try
    {
        return std::stoll(str);
    }
    catch (const std::exception &e)
    {
        AI_LOG_WARN("failed to parse timestamp '%s': %s", str.c_str(), e.what());
        return 0;
    }
}

std::string Minidump::getDestinationFile(int fd)
{
    // Use the modification time of the anonymous file (set when breakpad writes the
    // minidump at crash time) rather than the current time which is some variable
    // delay later when the postHalt hook runs.
    // We need millisecond resolution because AppService can update the
    // fireboltState annotation within a few ms of the crash.
    long long crashTimeMs = 0;
    std::time_t currentTime;
    struct stat st;
    if (fd >= 0 && fstat(fd, &st) == 0 && st.st_mtime != 0)
    {
        currentTime = st.st_mtime;
        crashTimeMs = static_cast<long long>(st.st_mtim.tv_sec) * 1000
                    + st.st_mtim.tv_nsec / 1000000;
    }
    else
    {
        AI_LOG_WARN("failed to get mtime from minidump fd, falling back to current time");
        auto now = std::chrono::system_clock::now();
        currentTime = std::chrono::system_clock::to_time_t(now);
        crashTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          now.time_since_epoch()).count();
    }
    std::stringstream timeString;
    timeString << std::put_time(std::localtime(&currentTime), "%FT%T");
    std::string destFile;
    std::string fileName;

    std::string dir(mContainerConfig->rdk_plugins->minidump->data->destination_path);

    std::map<std::string, std::string> annotations = mUtils->getAnnotations();

    // Only use the fireboltState annotation if it was set BEFORE the crash.
    // AppService often transitions the app to "background" after the crash but
    // before postHalt runs, which would record the wrong state.  If the current
    // annotation is stale, fall back to the previous value (which was the state
    // at the time of the crash, e.g. "foreground").
    auto it = annotations.find(FIREBOLT_STATE);
    if (it != annotations.end())
    {
        auto tsIt = annotations.find(FIREBOLT_STATE_TS);
        if (tsIt != annotations.end())
        {
            long long annotationMs = parseTimestampMs(tsIt->second);
            if (annotationMs > 0 && annotationMs > crashTimeMs)
            {
                AI_LOG_INFO("Current fireboltState '%s' was set after crash "
                            "(annotation_ms=%lld, crash_ms=%lld) - checking previous value",
                            it->second.c_str(),
                            annotationMs,
                            crashTimeMs);

                // Try the previous annotation value that was valid at crash time
                auto prevIt = annotations.find(FIREBOLT_STATE_PREV);
                auto prevTsIt = annotations.find(FIREBOLT_STATE_PREV_TS);
                if (prevIt != annotations.end() && prevTsIt != annotations.end())
                {
                    long long prevMs = parseTimestampMs(prevTsIt->second);
                    if (prevMs > 0 && prevMs <= crashTimeMs)
                    {
                        AI_LOG_INFO("Using previous fireboltState '%s' (set_ms=%lld, crash_ms=%lld)",
                                    prevIt->second.c_str(),
                                    prevMs,
                                    crashTimeMs);
                        it = prevIt;
                    }
                    else
                    {
                        it = annotations.end(); // both are after crash or parse failed
                    }
                }
                else
                {
                    it = annotations.end(); // no previous value available
                }
            }
        }
    }

    std::string containerId = mUtils->getContainerId();
    if (containerId.find("apps_") != std::string::npos) {
        //remove prefix before "apps_" from containerId
        containerId = containerId.substr(containerId.find("apps_"));
    }
    if (it != annotations.end()) {
        fileName = containerId + MINIDUMP_FN_SEPERATOR + it->second.c_str() + MINIDUMP_FN_SEPERATOR + timeString.str();
        if (fileName.length() > MINIDUMP_FILENAME_LENGTH)
            fileName.resize(MINIDUMP_FILENAME_LENGTH);
        destFile = dir + "/" + fileName + ".dmp";
        AI_LOG_INFO("Firebolt state: %s, minidump filename: %s", it->second.c_str(), destFile.c_str());
    } else {
        AI_LOG_INFO("Firebolt state not found or not valid at crash time");
        fileName = containerId + MINIDUMP_FN_SEPERATOR + timeString.str();
        if (fileName.length() > MINIDUMP_FILENAME_LENGTH)
            fileName.resize(MINIDUMP_FILENAME_LENGTH);
        destFile = dir + "/" + fileName + ".dmp";
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

