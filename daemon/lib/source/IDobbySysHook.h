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
 * File:   IDobbySysHook.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef IDOBBYSYSHOOK_H
#define IDOBBYSYSHOOK_H

#include <ContainerId.h>
#include "DobbyStartState.h"
#include "DobbyConfig.h"
#include "DobbyRootfs.h"

#include <sys/types.h>

#include <string>
#include <memory>



// -----------------------------------------------------------------------------
/**
 *  @class IDobbySysHook
 *  @brief Like IDobbyPlugin but an interface used by 'system' hooks (i.e. those
 *  built into the daemon rather than plugins).
 *
 *  Some examples of system hooks are the; LoopMounter, ResolvConf & NatNetwork.
 *
 *  System hooks are always called before any plugin hooks, except for in the
 *  preDestruction phase where they are called after all plugin hooks.
 *
 *
 */
class IDobbySysHook
{
public:
    virtual ~IDobbySysHook() = default;

public:

    // -------------------------------------------------------------------------
    /**
     *  @brief Should return a name for the hook.
     *
     *  This is only used for logging and thread names if the hook is running
     *  asynchronously.
     *
     *  @return name of the hook.
     */
    virtual std::string hookName() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Bit flags that should be returned by hookHints.
     *
     *  The flags are fairly self explainatory.
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
     *  hook.
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
     *  before the container is started.
     *
     *  At this point you can setup mounts that the container will see as it
     *  hasn't yet created the namespaces.
     *
     *
     *  @param[in]  id          The string id of the container.
     *  @param[in]  rootfsPath  The absolute path to the rootfs of the container.
     *  @param[in]  jsonData    The json data from the spec file.
     *
     *  @return true on success, false on failure.
     */
    virtual bool postConstruction(const ContainerId& id,
                                  const std::shared_ptr<IDobbyStartState>& startupState,
                                  const std::shared_ptr<const DobbyConfig>& config,
                                  const std::shared_ptr<const DobbyRootfs>& rootfs) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Hook function called after the container is setup, but before
     *  the init process is executed.
     *
     *  The hooks are run after the mounts are setup, but before we switch to
     *  the new root, so that the old root is still available in the hooks for
     *  any mount manipulations.
     *
     *  @param[in]  pid         The pid owner of the namespace to enter,
     *                          typically the pid of the process in the
     *                          container.
     *  @param[in]  nsType      The type of the namespace to enter, see above.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    virtual bool preStart(const ContainerId& id,
                          pid_t containerPid,
                          const std::shared_ptr<const DobbyConfig>& config,
                          const std::shared_ptr<const DobbyRootfs>& rootfs) = 0;

    virtual bool postStart(const ContainerId& id,
                           pid_t containerPid,
                           const std::shared_ptr<const DobbyConfig>& config,
                           const std::shared_ptr<const DobbyRootfs>& rootfs) = 0;

    virtual bool postStop(const ContainerId& id,
                          const std::shared_ptr<const DobbyConfig>& config,
                          const std::shared_ptr<const DobbyRootfs>& rootfs) = 0;

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
                                const std::shared_ptr<const DobbyConfig>& config,
                                const std::shared_ptr<const DobbyRootfs>& rootfs) = 0;

};


#endif // !defined(IDOBBYSYSHOOK_H)
