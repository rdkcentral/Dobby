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
 * File:   IDobbyPlugins.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef IDOBBYPLUGIN_H
#define IDOBBYPLUGIN_H

#include "ContainerId.h"
#include "IDobbyEnv.h"
#include "IDobbyUtils.h"
#include "IDobbyStartState.h"

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

#include <sys/types.h>

#include <string>
#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @class IDobbyPlugin
 *  @brief Interface that plugin libraries have to implement.
 *
 *
 */
class IDobbyPlugin
{
public:
    virtual ~IDobbyPlugin() = default;

public:
    // -------------------------------------------------------------------------
    /**
     *  @brief Should return the name of the plugin, this is used to match
     *  against the json spec file used to create the container.
     *
     *  The value returned should be constant for the lifetime of the class,
     *  as the value may be cached by the daemon at startup.
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
        PostConstructionSync    = (1 << 0),
        PreStartSync            = (1 << 1),
        PostStartSync           = (1 << 2),
        PostStopSync            = (1 << 3),
        PreDestructionSync      = (1 << 4),

        PostConstructionAsync   = (1 << 16),
        PreStartAsync           = (1 << 17),
        PostStartAsync          = (1 << 18),
        PostStopAsync           = (1 << 19),
        PreDestructionAsync     = (1 << 20)
    };

    // -------------------------------------------------------------------------
    /**
     *  @brief Should return a bitfield of the hook points implemented by the
     *  plugin.
     *
     *  Only bits that are set will be called as hooks.  This is to optimise
     *  the implementation of the hook code in the daemon and means threads
     *  aren't spawned for null hook points.
     *
     *  The value returned should be constant for the lifetime of the class,
     *  as the value may be cached by the daemon at startup.
     *
     *  @return bitmask of the HintFlags bits.
     */
    virtual unsigned hookHints() const = 0;

public:

    // -------------------------------------------------------------------------
    /**
     *  @brief Hook function called after the rootfs has been created, but
     *  before the container is launched.
     *
     *  At this point you can setup mounts that the container will see as it
     *  hasn't yet created the namespaces.  This is typically used to mount
     *  something that wouldn't be allowed inside a userns, then once mounted
     *  you can hook the prestart phase to move mount it into the container's
     *  namespace.  @see LoopMounterHook class for where this is used.
     *
     *  You can also hook this point to allow you to add extra environment
     *  variables or pass in additional file descriptors vie the @a startupState
     *  class.
     *
     *  @param[in]  id              The string id of the container.
     *  @param[in]  startupState    A utility object with methods that allow you
     *                              to tweak the parameters of the container
     *                              before it is launched.
     *  @param[in]  rootfsPath      The absolute path to the rootfs of the
     *                              container.
     *  @param[in]  jsonData        The json data from the spec file.
     *
     *  @return true on success, false on failure.
     */
    virtual bool postConstruction(const ContainerId& id,
                                  const std::shared_ptr<IDobbyStartState>& startupState,
                                  const std::string& rootfsPath,
                                  const Json::Value& jsonData) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Hook function called after the container is setup, but before
     *  the init process is executed.
     *
     *  The hooks are run after the mounts are setup, but before we switch to
     *  the new root, so that the old root is still available in the hooks for
     *  any mount manipulations.
     *
     *  @param[in]  id          The string id of the container.
     *  @param[in]  pid         The pid owner of the namespace to enter,
     *                          typically the pid of the process in the
     *                          container.
     *  @param[in]  nsType      The type of the namespace to enter, see above.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    virtual bool preStart(const ContainerId& id,
                          pid_t pid,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Hook function called after the container is started and the
     *  init process is now running.
     *
     *  This hook is not particularly useful, although it can be used to inform
     *  clients that a container has started successifully.
     *
     *  @param[in]  id          The id of the container (string)
     *  @param[in]  pid         The pid of the init process in the container.
     *  @param[in]  rootfsPath  The absolute path to the rootfs.
     *  @param[in]  jsonData    The json data for the hook specified in the
     *                          container spec file.
     *
     *  @return true on success, false on failure.
     */
    virtual bool postStart(const ContainerId& id,
                           pid_t pid,
                           const std::string& rootfsPath,
                           const Json::Value& jsonData) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Hook function called after the container has stopped.
     *
     *
     *
     *  @param[in]  id          The id of the container (string)
     *  @param[in]  rootfsPath  The absolute path to the rootfs.
     *  @param[in]  jsonData    The json data for the hook specified in the
     *                          container spec file.
     *
     *  @return true on success, false on failure.
     */
    virtual bool postStop(const ContainerId& id,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Hook function called just before the rootfs is deleted, this is
     *  called even if there was an error starting the container.
     *
     *  This hook is called at a very similar place to postStop, but it will
     *  be called even if the container failed to start (but as long as
     *  postConstruction was called).
     *
     *
     *  @param[in]  id          The string id of the container.
     *  @param[in]  rootfsPath  The absolute path to the rootfs of the container.
     *  @param[in]  jsonData    The json data from the spec file.
     *
     *  @return true on success, false on failure.
     */
    virtual bool preDestruction(const ContainerId& id,
                                const std::string& rootfsPath,
                                const Json::Value& jsonData) = 0;

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
#   define PUBLIC_FN __attribute__ ((visibility ("default")))
#else
#   define PUBLIC_FN
#endif


// -----------------------------------------------------------------------------
/**
 *  @define REGISTER_DOBBY_PLUGIN
 *  @brief Macro for plugins to use to register themselves
 *
 *
 *
 */
#define REGISTER_DOBBY_PLUGIN(_class, _args...) \
    int PUBLIC_FN __attribute__((weak)) __ai_debug_log_level = 0; \
    void PUBLIC_FN __attribute__((weak)) \
    __ai_debug_log_printf(int level, const char *file, const char *func, \
                          int line, const char *fmt, ...) \
    { } \
    void PUBLIC_FN __attribute__((weak)) \
    __ai_debug_log_sys_printf(int err, int level, const char *file, \
                              const char *func, int line, const char *fmt, ...) \
    { } \
    \
    extern "C" PUBLIC_FN IDobbyPlugin* createIDobbyPlugin(const std::shared_ptr<IDobbyEnv>& env, \
                                                          const std::shared_ptr<IDobbyUtils>& utils); \
    extern "C" PUBLIC_FN IDobbyPlugin* createIDobbyPlugin(const std::shared_ptr<IDobbyEnv>& env, \
                                                          const std::shared_ptr<IDobbyUtils>& utils) \
    { \
        return new _class(std::dynamic_pointer_cast<IDobbyEnv>(env), \
                          std::dynamic_pointer_cast<IDobbyUtils>(utils), \
                          ##_args); \
    } \
    extern "C" PUBLIC_FN void destroyIDobbyPlugin(_class const* _plugin); \
    extern "C" PUBLIC_FN void destroyIDobbyPlugin(_class const* _plugin) \
    { \
        return delete _plugin; \
    }



#endif // !defined(IDOBBYPLUGIN_H)
