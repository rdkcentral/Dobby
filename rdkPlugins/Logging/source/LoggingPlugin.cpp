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

#include "LoggingPlugin.h"

#include <fcntl.h>
#include <limits.h>
#include <algorithm>
#if defined(USE_SYSTEMD)
#include <systemd/sd-journal.h>
#endif
#include <sstream>
#include <string.h>
#include <map>

/**
 * Register the logging plugin with the special logging registration method
 */
REGISTER_RDK_LOGGER(LoggingPlugin);

/**
 * @brief Constructor - called when plugin is loaded by PluginLauncher
 *
 * Do not change the parameters for this constructor - must match C methods
 * created by REGISTER_RDK_PLUGIN macro
 *
 * Note plugin name is not case sensitive
 */
LoggingPlugin::LoggingPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                             const std::string &rootfsPath)
    : mName("Logging"),
      mContainerConfig(containerConfig),
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
unsigned LoggingPlugin::hookHints() const
{
    return IDobbyRdkPlugin::HintFlags::PostInstallationFlag;
}

// Begin Hook Methods

/**
 * @brief Set the correct options in the config file
 */
bool LoggingPlugin::postInstallation()
{
    AI_LOG_INFO("Running Logging postInstallation hook");

    // Redirect hook output to stdout/stderr
    if (mContainerConfig->annotations == nullptr)
    {
        mContainerConfig->annotations = (json_map_string_string *)calloc(1, sizeof(json_map_string_string));
    }
    mContainerConfig->annotations->keys =
        (char **)realloc(mContainerConfig->annotations->keys, sizeof(char *) * (mContainerConfig->annotations->len + 2));
    mContainerConfig->annotations->values =
        (char **)realloc(mContainerConfig->annotations->values, sizeof(char *) * (mContainerConfig->annotations->len + 2));

    mContainerConfig->annotations->keys[mContainerConfig->annotations->len] = strdup("run.oci.hooks.stderr");
    mContainerConfig->annotations->values[mContainerConfig->annotations->len] = strdup("/dev/stderr");
    mContainerConfig->annotations->keys[mContainerConfig->annotations->len + 1] = strdup("run.oci.hooks.stdout");
    mContainerConfig->annotations->values[mContainerConfig->annotations->len + 1] = strdup("/dev/stdout");

    mContainerConfig->annotations->len += 2;

    // We need to use an isolated terminal to give each container its own ptty
    mContainerConfig->process->terminal = true;

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
std::vector<std::string> LoggingPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_logging* pluginConfig = mContainerConfig->rdk_plugins->logging;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

/**
 * @brief Public method called by DobbyLogger to start running the logging
 * loop. Destination of the logs depends on the settings in the config file.
 *
 *
 * @param[in]  containerInfo Details about the container and socket connection
 * @param[in]  isBuffer      If true, containerInfo.pttyFd points to a memFd
 *                           containing a fixed size buffer of data that should
 *                           be read until EOF is reached, at which point this
 *                           method should end.
 * @param[in]  createNew     If true, create a new log file (if applicable) instead
 *                           of appending to an existing one
 */
void LoggingPlugin::LoggingLoop(ContainerInfo containerInfo,
                                const bool isBuffer,
                                const bool createNew,
                                const std::atomic_bool &cancellationToken)
{
    AI_LOG_FN_ENTRY();

    // Work out where to send the logs and start the loop to read/write logs
    // Will send to /dev/null if unknown sink
    auto loggingSink = GetContainerSink();
    switch (loggingSink)
    {
    case LoggingSink::File:
        // If we're not creating a new file, append to the existing log
        FileSink(containerInfo, isBuffer, createNew, cancellationToken);
        break;
    case LoggingSink::Journald:
#if defined(USE_SYSTEMD)
        JournaldSink(containerInfo, isBuffer, cancellationToken);
#else
        AI_LOG_ERROR("Logging plugin built without systemd support - cannot use journald");
        DevNullSink(containerInfo, isBuffer, cancellationToken);
#endif
        break;
    case LoggingSink::DevNull:
        DevNullSink(containerInfo, isBuffer, cancellationToken);
        break;
    default:
        break;
    }

    // Logging loop has finished, close the connection if needed
    if (containerInfo.connectionFd > 0 && close(containerInfo.connectionFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to close connection");
    }

    // If dumping a buffer, DobbyBufferStream will clean up after itself
    if (!isBuffer && containerInfo.pttyFd > 0 && fcntl(containerInfo.pttyFd, F_GETFD) != -1)
    {
        if (close(containerInfo.pttyFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container ptty fd");
        }
    }

    AI_LOG_FN_EXIT();
}

// Begin private methods

/**
 * @brief Converts the "sink: xxx" in the config to a valid log sink. Case
 * insensitive
 */
LoggingPlugin::LoggingSink LoggingPlugin::GetContainerSink()
{
    // Check the plugin data is actually there
    if (!mContainerConfig->rdk_plugins->logging->data)
    {
        AI_LOG_WARN("Logging config is null or could not be parsed - sending all logs to /dev/null");
        return LoggingSink::DevNull;
    }

    // Convert to lowercase
    std::string sinkString = mContainerConfig->rdk_plugins->logging->data->sink;
    std::transform(sinkString.begin(), sinkString.end(), sinkString.begin(), ::tolower);

    // Work out where to send the logs
    if (sinkString == "file")
    {
        return LoggingSink::File;
    }
    if (sinkString == "journald")
    {
        return LoggingSink::Journald;
    }
    if (sinkString == "devnull")
    {
        return LoggingSink::DevNull;
    }

    AI_LOG_WARN("Unknown logging sink - using /dev/null instead");
    return LoggingSink::DevNull;
}

#if defined(USE_SYSTEMD)
/**
 * @brief Send container logs to journald
 *
 * @param[in] containerInfo     Info about the container to log including fd
 *                              to read from
 * @param[in] exitEof   Whether this logging loop should finish when it
 *                      reaches the end of the file, or if it should keep looping
 *                      and wait for the fd to be deleted. Set to true when
 *                      dumping the contents of a buffer
 */
void LoggingPlugin::JournaldSink(const ContainerInfo &containerInfo, bool exitEof,
                                 const std::atomic_bool &cancellationToken)
{
    AI_LOG_INFO("starting logger for container '%s' to journald (PID: %d)",
                mUtils->getContainerId().c_str(), containerInfo.containerPid);

    char buf[8192];
    memset(buf, 0, sizeof(buf));

    ssize_t bytesRead;
    size_t bufferUsed = 0;
    size_t bufferRemaining = 0;

    // Read options from config to set the journald priority, default to LOG_INFO
    int logPriority = LOG_INFO;
    if (mContainerConfig->rdk_plugins->logging->data->journald_options)
    {
        std::string priority = mContainerConfig->rdk_plugins->logging->data->journald_options->priority;
        if (!priority.empty())
        {
            const std::map<std::string, int> options =
                {
                    {"LOG_EMERG", 0},
                    {"LOG_ALERT", 1},
                    {"LOG_CRIT", 2},
                    {"LOG_ERR", 3},
                    {"LOG_WARNING", 4},
                    {"LOG_NOTICE", 5},
                    {"LOG_INFO", 6},
                    {"LOG_DEBUG", 7}};

            auto it = options.find(priority);
            if (it != options.end())
            {
                logPriority = it->second;
            }
            else
            {
                AI_LOG_WARN("Could not parse journald priority - using LOG_INFO");
            }
        }
    }

    // Read from the fd until the file is closed
    // Journald expects messages to be single lines, so we need to process incoming
    // data to split into individual lines
    while (!cancellationToken)
    {
        // Work out how much space is left in the buffer and read as much as we can
        bufferRemaining = sizeof(buf) - bufferUsed;

        bytesRead = read(containerInfo.pttyFd, &buf[bufferUsed], bufferRemaining);
        if (bytesRead < 0)
        {
            AI_LOG_INFO("Container %s terminated, terminating logging thread",
                        mUtils->getContainerId().c_str());
            break;
        }
        if (bytesRead == 0 && exitEof)
        {
            break;
        }
        bufferUsed += bytesRead;

        // Loop through the received data, finding the position of new lines. Only
        // send complete lines to journald
        char *lineStart = buf;
        char *lineEnd;
        while ((lineEnd = static_cast<char *>(memchr(lineStart, '\n', bufferUsed - (lineStart - buf)))) != nullptr)
        {
            *lineEnd = 0;

            std::string msg(lineStart, strlen(lineStart));

            // Despite what documentation says, we need to remove any line break
            // characters. Only matched on the \n so we still need to remove the
            // \r if there is one
            if (msg.back() == '\r')
            {
                msg.pop_back();
            }

            if (!msg.empty())
            {
                // Note PID in "journalctl -f" will show as daemon PID, but when
                // viewing journald in full JSON format, the container PID is
                // visible
                sd_journal_send("MESSAGE=%s", msg.c_str(),
                                "PRIORITY=%i", logPriority,
                                "SYSLOG_IDENTIFIER=%s", mUtils->getContainerId().c_str(),
                                "OBJECT_PID=%ld", containerInfo.containerPid,
                                "SYSLOG_PID=%ld", containerInfo.containerPid,
                                NULL);
            }

            lineStart = lineEnd + 1;
        }

        // Shift buffer down so the unprocessed data is at the start
        bufferUsed -= (lineStart - buf);
        memmove(buf, lineStart, bufferUsed);
    }

    if (cancellationToken)
    {
        AI_LOG_INFO("Logging thread shut down by cancellation token");
    }
}
#endif //#if defined(USE_SYSTEMD)

/**
 * @brief Send container logs to /dev/null.
 *
 * @param[in] containerInfo     Info about the container to log including fd
 *                              to read from
 * @param[in] exitEof   Whether this logging loop should finish when it
 *                      reaches the end of the file, or if it should keep looping
 *                      and wait for the fd to be deleted. Set to true when
 *                      dumping the contents of a buffer
 */
void LoggingPlugin::DevNullSink(const ContainerInfo &containerInfo, bool exitEof,
                                const std::atomic_bool &cancellationToken)
{
    int devNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);

    if (devNullFd < 0)
    {
        AI_LOG_WARN("Could not open /dev/null");
        return;
    }

    AI_LOG_INFO("starting logger for container '%s' to /dev/null",
                mUtils->getContainerId().c_str());

    char buf[8192];
    memset(buf, 0, sizeof(buf));

    ssize_t ret;

    // Read from the fd until the file is closed
    while (!cancellationToken)
    {
        // FD is blocking, so this will just block this thread until there
        // is data to be read
        ret = read(containerInfo.pttyFd, buf, sizeof(buf));
        if (ret < 0)
        {
            AI_LOG_INFO("Container %s terminated, terminating logging thread",
                        mUtils->getContainerId().c_str());
            break;
        }
        if (ret == 0 && exitEof)
        {
            break;
        }
        write(devNullFd, buf, ret);
    }


    if (cancellationToken)
    {
        AI_LOG_INFO("Logging thread shut down by cancellation token");
    }

    // Close /dev/null
    if (close(devNullFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to close /dev/null");
    }
}

/**
 * @brief Write container logs to the file specified in the config
 *
 * @param[in] containerInfo     Info about the container to log including fd
 *                              to read from
 * @param[in] exitEof   Whether this logging loop should finish when it
 *                      reaches the end of the file, or if it should keep looping
 *                      and wait for the fd to be deleted. Set to true when
 *                      dumping the contents of a buffer
 * @param[in] createNew Whether to create a new empty log file or append to an
 *                      existing file if it already exists
 */
void LoggingPlugin::FileSink(const ContainerInfo &containerInfo, bool exitEof, bool createNew,
                             const std::atomic_bool &cancellationToken)
{
    const mode_t mode = 0644;

    int flags;
    // Do we want to append to the file or create a new empty file?
    if (createNew)
    {
        flags = O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC;
    }
    else
    {
        flags = O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC;
    }

    // Read the options from the config if possible
    std::string pathName;
    ssize_t limit = -1;

    if (mContainerConfig->rdk_plugins->logging->data->file_options)
    {
        pathName = mContainerConfig->rdk_plugins->logging->data->file_options->path;
        if (mContainerConfig->rdk_plugins->logging->data->file_options->limit_present)
        {
            limit = mContainerConfig->rdk_plugins->logging->data->file_options->limit;
        }
    }

    // if limit is -1 it means unlimited, but to make life easier just set it
    // to the max value
    if (limit < 0)
        limit = SSIZE_MAX;

    // Open both our file and /dev/null (so we can send to null if we hit the limit)
    int outputFd = -1;
    int devNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);

    if (pathName.empty())
    {
        AI_LOG_ERROR("Log settings set to log to file but no path provided. Sending to /dev/null");
        outputFd = devNullFd;
    }
    else
    {
        outputFd = open(pathName.c_str(), flags, mode);
        if (outputFd < 0)
        {
            // we continue creating the thread, it just means the thread will
            // throw away everything it receives
            AI_LOG_SYS_ERROR(errno, "failed to open/create '%s'", pathName.c_str());
            outputFd = devNullFd;
        }
    }

    AI_LOG_DEBUG("starting logger for container '%s' to write to '%s' (limit %zd bytes)",
                mUtils->getContainerId().c_str(), pathName.c_str(), limit);

    // TODO:: Replace with splice/sendfile to avoid copying data in and out of
    // userspace. This should perform OK for our needs for now though
    char buf[8192];
    memset(buf, 0, sizeof(buf));

    ssize_t ret;
    ssize_t offset = 0;

    bool limitHit = false;

    // Read from the fd until the file is closed
    while (!cancellationToken)
    {
        // FD is blocking, so this will just block this thread until there
        // is data to be read
        ret = read(containerInfo.pttyFd, buf, sizeof(buf));
        if (ret < 0)
        {
            AI_LOG_INFO("Container %s terminated, terminating logging thread",
                        mUtils->getContainerId().c_str());
            break;
        }
        if (ret == 0 && exitEof)
        {
            break;
        }

        offset += ret;

        // Have we hit the size limit?
        if (offset <= limit)
        {
            // Write to the output file
            write(outputFd, buf, ret);
        }
        else
        {
            // Hit the limit, send the data into the void
            if (!limitHit)
            {
                AI_LOG_WARN("Logger for container %s has hit maximum size of %zu",
                            mUtils->getContainerId().c_str(), limit);
            }
            limitHit = true;
            write(devNullFd, buf, ret);
        }
    }

    if (cancellationToken)
    {
        AI_LOG_INFO("Logging thread shut down by cancellation token");
    }

    // Separate sections of log file for reabability
    // (useful if we're writing lots of buffer dumps)
    std::string marker = "---------------------------------------------\n";
    write(outputFd, marker.c_str(), marker.length());

    // Close the logfile
    if (outputFd >= 0 && close(outputFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to close file");
    }
}