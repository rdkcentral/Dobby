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
#include <sstream>
#include <string.h>
#include <map>

#if defined(USE_SYSTEMD)
#include "JournaldSink.h"
#endif
#include "FileSink.h"
#include "NullSink.h"

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

LoggingPlugin::~LoggingPlugin()
{
    AI_LOG_FN_ENTRY();

    // Make sure we clean up after ourselves
    if (mSink && mPollLoop)
    {
        if (mPollLoop->hasSource(mSink))
        {
            mPollLoop->delSource(mSink);
        }
    }

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

    // Plugin launcher will automatically send hook output to journald so don't
    // want to duplicate it by capturing it here too
    if (GetContainerSink() != LoggingSink::Journald)
    {
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
    }

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
    const rt_defs_plugins_logging *pluginConfig = mContainerConfig->rdk_plugins->logging;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

/**
 * @brief Adds the necessary poll source(s) to the provided pollLoop instance
 * based on the logging sink specified in the container config
 *
 * @param[in]   fd              The file descriptor we need to read from (i.e. the container tty)
 * @param[in]   pollLoop        The poll loop the sources should be added to
 */
void LoggingPlugin::RegisterPollSources(int fd, std::shared_ptr<AICommon::IPollLoop> pollLoop)
{
    AI_LOG_FN_ENTRY();

    // If we haven't already created a sink, then create one
    if (!mSink)
    {
        mSink = CreateSink(GetContainerSink());

        if (mSink == nullptr)
        {
            AI_LOG_ERROR_EXIT("Failed to create container sink - cannot setup logging");
            return;
        }
    }

    if (!mPollLoop)
    {
        mPollLoop = pollLoop;
    }

    // Register the poll source
    if (!mPollLoop->addSource(mSink, fd, EPOLLIN))
    {
        AI_LOG_ERROR("Failed to add logging poll source for container %s", mUtils->getContainerId().c_str());
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Dump the contents of a file descriptor to the log sink
 *
 * Will block until the contents of the fd has been written to the
 * log
 *
 * @param[in]   bufferFd    The file descriptor to read from
 */
void LoggingPlugin::DumpToLog(const int bufferFd)
{
    AI_LOG_FN_ENTRY();

    if (!mSink)
    {
        mSink = CreateSink(GetContainerSink());
    }

    // Block and write the contents of the bufferFd to the
    // log sink
    mSink->DumpLog(bufferFd);

    AI_LOG_FN_EXIT();
}

// Begin private methods

/**
 * @brief Constructs an instance of the requested sink
 *
 * @param[in]  sinkType    The type of sink to be constructed
 *
 * @return shared_ptr pointing to the instance of the logging sink. Nullptr if no sink
 * of the requested type is available
 */
std::shared_ptr<ILoggingSink> LoggingPlugin::CreateSink(LoggingPlugin::LoggingSink sinkType)
{
    switch (sinkType)
    {
    case LoggingSink::Journald:
#ifdef USE_SYSTEMD
        return std::make_shared<JournaldSink>(mUtils->getContainerId(), mContainerConfig);
#else
        AI_LOG_ERROR("Cannot create journald sink - Dobby built without systemd support");
        return std::make_shared<NullSink>(mUtils->getContainerId(), mContainerConfig);
#endif
    case LoggingSink::File:
        return std::make_shared<FileSink>(mUtils->getContainerId(), mContainerConfig);
    case LoggingSink::DevNull:
        return std::make_shared<NullSink>(mUtils->getContainerId(), mContainerConfig);
    default:
        AI_LOG_ERROR("Could not create sink - unknown sink type");
        return nullptr;
    }
}

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