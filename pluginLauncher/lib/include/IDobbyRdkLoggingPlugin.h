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
/*
 * File:   IDobbyRdkLoggingPlugin.h
 *
 */
#ifndef IDOBBYRDKLOGGINGPLUGIN_H
#define IDOBBYRDKLOGGINGPLUGIN_H

#include "IDobbyRdkPlugin.h"
#include "DobbyRdkPluginUtils.h"

#include "rt_dobby_schema.h"

#include <Logging.h>
#include <json/json.h>
#include <sys/types.h>

#include <string>
#include <memory>
#include <future>

// -----------------------------------------------------------------------------
/**
 *  @class IDobbyRdkLoggingPlugin
 *  @brief Interface that plugin libraries have to implement.
 *
 *
 */
class IDobbyRdkLoggingPlugin : public IDobbyRdkPlugin
{
public:
    virtual ~IDobbyRdkLoggingPlugin() = default;

public:
    struct ContainerInfo
    {
        // Actual pid of the running container
        pid_t containerPid;
        // fd of the open connection so we can close it when the container exits
        int connectionFd;
        // fd of the container pseudo-terminal master fd
        int pttyFd;
    };

    virtual void LoggingLoop(ContainerInfo containerInfo,
                             const bool isBuffer,
                             const bool createNew) = 0;
};

// -----------------------------------------------------------------------------
/**
 *  @define REGISTER_RDK_LOGGER
 *  @brief Macro for logging plugins to use to register themselves
 *
 *  Needed because we want to be able to distinguish logging plugins
 *  from normal plugins - logging plugins must have some additional methods
 *
 *  Plugin manager searches for libraries that implement these C methods
 *  so without calling this macro the plugin will not be loaded. Need to be
 *  C methods to work around c++ name-mangling
 *
 */
#define REGISTER_RDK_LOGGER(_class)                                                                                                                                                                                                        \
    extern "C" PUBLIC_FN IDobbyRdkLoggingPlugin *createIDobbyRdkLogger(std::shared_ptr<rt_dobby_schema> &containerConfig, const std::shared_ptr<DobbyRdkPluginUtils> &utils, const std::string &rootfsPath); \
    extern "C" PUBLIC_FN IDobbyRdkLoggingPlugin *createIDobbyRdkLogger(std::shared_ptr<rt_dobby_schema> &containerConfig, const std::shared_ptr<DobbyRdkPluginUtils> &utils, const std::string &rootfsPath)  \
    {                                                                                                                                                                                                                                      \
        return new _class(containerConfig, utils, rootfsPath);                                                                                                                                                                  \
    }                                                                                                                                                                                                                                      \
    extern "C" PUBLIC_FN void destroyIDobbyRdkLogger(_class const *_plugin);                                                                                                                                                               \
    extern "C" PUBLIC_FN void destroyIDobbyRdkLogger(_class const *_plugin)                                                                                                                                                                \
    {                                                                                                                                                                                                                                      \
        return delete _plugin;                                                                                                                                                                                                             \
    }

#endif // !defined(IDOBBYRDKLOGGINGPLUGIN_H)
