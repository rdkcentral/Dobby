/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * File:   IDobbyRdkPlugin.h
 *
 */
#ifndef IDobbyRdkPlugin_H
#define IDobbyRdkPlugin_H

#include "DobbyRdkPluginUtils.h"

#include "rt_dobby_schema.h"

#include <Logging.h>
#include <json/json.h>
#include <sys/types.h>

#include <string>
#include <memory>

// -----------------------------------------------------------------------------
/**
 *  @class IDobbyRdkPlugin
 *  @brief Interface that plugin libraries have to implement.
 *
 *
 */
class IDobbyRdkPlugin
{
public:
    virtual ~IDobbyRdkPlugin() = default;

public:
    // -------------------------------------------------------------------------
    /**
     *  @brief Should return the name of the plugin
     *
     *  @return string with the name of the hook.
     */
    virtual std::string name() const = 0;

public:
    // -------------------------------------------------------------------------
    /**
     *  @brief Bit flags that should be returned by hookHints.
     *
     *  The flags are fairly self explanatory.
     */
    enum HintFlags : unsigned
    {
        PostInstallationFlag = (1 << 0),
        PreCreationFlag = (1 << 1),
        CreateRuntimeFlag = (1 << 2),
        CreateContainerFlag = (1 << 3),
        StartContainerFlag = (1 << 4),
        PostStartFlag = (1 << 5),
        PostHaltFlag = (1 << 6),
        PostStopFlag = (1 << 7),
        Unknown = 0
    };

    // -------------------------------------------------------------------------
    /**
     *  @brief Should return a bitfield of the hook points implemented by the
     *  plugin.
     *
     *  Only bits that are set will be called as hooks.  This is to optimise
     *  the implementation of the hook code to ensure we don't waste time
     *  trying to run hooks that don't do anything
     *
     *  The value returned should be constant for the lifetime of the class
     *
     *  @return bitmask of the HintFlags bits.
     */
    virtual unsigned hookHints() const = 0;

public:
    // Dobby hook
    virtual bool postInstallation() = 0;

    // Dobby hook
    virtual bool preCreation() = 0;

    // OCI Hook
    virtual bool createRuntime() = 0;

    // OCI Hook
    virtual bool createContainer() = 0;

    // OCI Hook
    virtual bool startContainer() = 0;

    // OCI Hook
    virtual bool postStart() = 0;

    // Dobby hook
    virtual bool postHalt() = 0;

    // OCI Hook (called after delete)
    virtual bool postStop() = 0;
};

// -----------------------------------------------------------------------------
/**
 *  @define PUBLIC_FN
 *  @brief Macro for setting the visiblity on the symbols
 *
 *  See https://gcc.gnu.org/wiki/Visibility for details on why this is a good
 *  idea.
 *
 */
#if __GNUC__ >= 4
#define PUBLIC_FN __attribute__((visibility("default")))
#else
#define PUBLIC_FN
#endif

// -----------------------------------------------------------------------------
/**
 *  @define REGISTER_RDK_PLUGIN
 *  @brief Macro for plugins to use to register themselves
 *
 *  Plugin manager searches for libraries that implement these C methods
 *  so without calling this macro the plugin will not be loaded. Need to be
 *  C methods to work around c++ name-mangling
 *
 */
#define REGISTER_RDK_PLUGIN(_class)                                          \
    extern "C" PUBLIC_FN IDobbyRdkPlugin *createIDobbyRdkPlugin(std::shared_ptr<rt_dobby_schema>& containerConfig, const std::shared_ptr<DobbyRdkPluginUtils> &utils, const std::string& rootfsPath);   \
    extern "C" PUBLIC_FN IDobbyRdkPlugin *createIDobbyRdkPlugin(std::shared_ptr<rt_dobby_schema>& containerConfig, const std::shared_ptr<DobbyRdkPluginUtils> &utils, const std::string& rootfsPath)    \
    {                                                                        \
        return new _class(containerConfig, utils, rootfsPath);               \
    }                                                                        \
    extern "C" PUBLIC_FN void destroyIDobbyRdkPlugin(_class const *_plugin); \
    extern "C" PUBLIC_FN void destroyIDobbyRdkPlugin(_class const *_plugin)  \
    {                                                                        \
        return delete _plugin;                                               \
    }

#endif // !defined(IDobbyRdkPlugin_H)
