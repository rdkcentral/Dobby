/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   EthanLogPlugin.cpp
 *
 */
#include "EthanLogPlugin.h"
#include "EthanLogLoop.h"
#include "EthanLogClient.h"

#include <Logging.h>

#include <sstream>

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/file.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Registers the main plugin object.
 *
 *  The object is constructed at the start of the daemon and only destructed
 *  when the daemon is shutting down.
 *
 */
REGISTER_DOBBY_PLUGIN(EthanLogPlugin)


// -----------------------------------------------------------------------------
/**
 *  @brief Default logging levels bit mask
 *
 *  The defaults are determined by build type
 *
 */
#if (AI_BUILD_TYPE == AI_DEBUG)
#   define DEFAULT_LOG_LEVELS   (unsigned)(EthanLogClient::LOG_LEVEL_FATAL   | \
                                           EthanLogClient::LOG_LEVEL_ERROR   | \
                                           EthanLogClient::LOG_LEVEL_WARNING | \
                                           EthanLogClient::LOG_LEVEL_INFO    | \
                                           EthanLogClient::LOG_LEVEL_DEBUG   | \
                                           EthanLogClient::LOG_LEVEL_MILESTONE)
#elif (AI_BUILD_TYPE == AI_RELEASE)
#   define DEFAULT_LOG_LEVELS   (unsigned)(0)
#else
#   error AI_BUILD_TYPE must be either AI_DEBUG or AI_RELEASE
#endif




EthanLogPlugin::EthanLogPlugin(const std::shared_ptr<IDobbyEnv>& env,
                               const std::shared_ptr<IDobbyUtils>& utils)
    : mName("EthanLog")
    , mUtilities(utils)
    , mLogLoop(std::make_shared<EthanLogLoop>())
    , mDefaultLogLevelsMask(DEFAULT_LOG_LEVELS)
    , mDevNullFd(-1)
{
    AI_LOG_FN_ENTRY();

    // open /dev/null, we use this rather than a pipe if no log levels are
    // enabled (this is common on production builds)
    mDevNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);
    if (mDevNullFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open /dev/null ?");
    }

    AI_LOG_FN_EXIT();
}

EthanLogPlugin::~EthanLogPlugin()
{
    AI_LOG_FN_ENTRY();

    if ((mDevNullFd >= 0) && (close(mDevNullFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close file");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Boilerplate that just returns the name of the hook
 *
 *  This string needs to match the name specified in the container spec json.
 *
 */
std::string EthanLogPlugin::name() const
{
    return mName;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Indicates which hook points we want and whether to run the
 *  asynchronously or synchronously with the other hooks
 *
 *  For this plugin everything is done in the postConstruction and preStart
 *  hooks and synchronously with any other plugins / hooks.
 */
unsigned EthanLogPlugin::hookHints() const
{
    return IDobbyPlugin::PostConstructionSync |
           IDobbyPlugin::PreStartSync;
}

// -----------------------------------------------------------------------------
/**
 *  @brief We hook the postConstruction point to create a logging pipe for the
 *  container which we then given to the init process of the container.
 *
 *  The json for the plugin data should be formatted like so
 *
 *      {
 *          "name" : "SomeApp",
 *          "loglevels" : [ "fatal", "milestone", "info", "debug" ],
 *          "rateLimit" { "rate": 1234, "burstSize": 12345 }
 *      }
 *
 *
 *  @param[in]  id              The id of the container
 *  @param[in]  startupState    The startup state of the container (ignored)
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container
 *  @param[in]  jsonData        The json data from the spec file (ignored)
 *
 *  @return true if the hardlink ws created, otherwise false.
 */
bool EthanLogPlugin::postConstruction(const ContainerId& id,
                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                      const std::string& rootfsPath,
                                      const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    std::string logName;
    uint32_t logLevelsMask;

    // parse the supplied json data to determine the args to send over dbus
    if (jsonData.isNull())
    {
        // if no data was supplied use default values
        logName = id.str();
        logLevelsMask = mDefaultLogLevelsMask;
    }
    else if (jsonData.isObject())
    {
        // get the name field if present
        const Json::Value& appName = jsonData["name"];
        if (appName.isNull())
        {
            logName = id.str();
        }
        else if (appName.isString())
        {
            logName = appName.asString();
        }
        else
        {
            AI_LOG_ERROR_EXIT("invalid 'name' field for plugin");
            return false;
        }

        // get the log levels array if present
        const Json::Value& logLevels = jsonData["loglevels"];
        if (logLevels.isNull())
        {
            logLevelsMask = mDefaultLogLevelsMask;
        }
        else if (logLevels.isArray())
        {
            logLevelsMask = parseLogLevels(logLevels);
        }
        else
        {
            AI_LOG_ERROR_EXIT("invalid 'loglevels' field for plugin");
            return false;
        }

        // TODO: add rateLimit parsing
    }
    else
    {
        AI_LOG_ERROR_EXIT("plugin data is not an object and therefore ill-formed");
        return false;
    }


    // set the default logging pipe as /dev/null, will be overridden if we
    // managed to create logging pipe
    int pipeFd = mDevNullFd;

    // only go to the trouble of creating the logging pipe if we have at least
    // one log level enabled
    if (logLevelsMask != 0)
    {
        pipeFd = mLogLoop->addClient(id, logName, logLevelsMask);
        if (pipeFd < 0)
        {
            AI_LOG_ERROR("failed to create logging pipe for '%s'", logName.c_str());
            pipeFd = mDevNullFd;
        }
    }


    // add the fd to the container start-up state
    int containerFd = startupState->addFileDescriptor(pipeFd);
    if (containerFd < 0)
    {
        AI_LOG_ERROR("failed to added logging pipe fd to the container");
    }
    else
    {
        // add the environment var informing where the pipe is
        std::ostringstream envVar;
        envVar << "ETHAN_LOGGING_PIPE=" << containerFd;

        if (!startupState->addEnvironmentVariable(envVar.str()))
        {
            AI_LOG_ERROR("failed to add environment var for logging");
        }
    }


    // close the fd if not /dev/null (start state has dup'd the fd)
    if ((pipeFd != mDevNullFd) && (close(pipeFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close logging pipe");
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief We hook the preStart point so we can tell the EthanLog code the base
 *  pid of the container.
 *
 *  We need this because the logging library reports it's own pid and that is
 *  relative to it's own pid namespace.
 *
 *  @param[in]  id              The id of the container
 *  @param[in]  pid             The pid of the init process inside the container
 *  @param[in]  rootfsPath      The absolute path to the rootfs of the container
 *  @param[in]  jsonData        The json data from the spec file (ignored)
 *
 *  @return Always return true.
 */
bool EthanLogPlugin::preStart(const ContainerId& id,
                              pid_t pid,
                              const std::string& rootfsPath,
                              const Json::Value& jsonData)
{
    AI_LOG_FN_ENTRY();

    (void) rootfsPath;
    (void) jsonData;

    mLogLoop->setClientBasePid(id, pid);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Parses the supplied json array and returns a bitmask of the log
 *  levels
 *
 *  The json object should be an array of strings which may contain any of
 *  the following strings
 *
 *      "default", "fatal", "error", "warning", "info", "debug", "milestone"
 *
 *
 *  @param[in]  jsonArray       The json array object
 *
 *  @return the bitmask of log levels, maybe 0 if the array was empty.
 */
unsigned EthanLogPlugin::parseLogLevels(const Json::Value& jsonArray) const
{
    unsigned levelsMask = 0;

    for (const Json::Value &value : jsonArray)
    {
        if (!value.isString())
        {
            AI_LOG_ERROR("invalid entry in the loglevels json array");
        }
        else
        {
            const char* value_ = value.asCString();
            if (strcasecmp(value_, "default") == 0)
            {
                levelsMask |= mDefaultLogLevelsMask;
            }
            else if (strcasecmp(value_, "fatal") == 0)
            {
                levelsMask |= EthanLogClient::LOG_LEVEL_FATAL;
            }
            else if (strcasecmp(value_, "error") == 0)
            {
                levelsMask |= EthanLogClient::LOG_LEVEL_ERROR;
            }
            else if (strcasecmp(value_, "warning") == 0)
            {
                levelsMask |= EthanLogClient::LOG_LEVEL_WARNING;
            }
            else if (strcasecmp(value_, "info") == 0)
            {
                levelsMask |= EthanLogClient::LOG_LEVEL_INFO;
            }
            else if (strcasecmp(value_, "debug") == 0)
            {
                levelsMask |= EthanLogClient::LOG_LEVEL_DEBUG;
            }
            else if (strcasecmp(value_, "milestone") == 0)
            {
                levelsMask |= EthanLogClient::LOG_LEVEL_MILESTONE;
            }
            else
            {
                AI_LOG_WARN("unknown log level string '%s'", value_);
            }
        }
    }

    return levelsMask;
}

