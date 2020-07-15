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
 * File:   IDobbySettings.h
 *
 * Copyright (C) BSKYB 2020+
 */
#ifndef IDOBBYSETTINGS_H
#define IDOBBYSETTINGS_H

#include <string>
#include <list>
#include <map>
#include <set>

// -----------------------------------------------------------------------------
/**
 *  @class IDobbySettings
 *  @brief Interface provided to the library at startup, contains the
 *  configuration options for Dobby.
 *
 *
 *
 */
class IDobbySettings
{
public:
    virtual ~IDobbySettings() = default;

public:

    // -------------------------------------------------------------------------
    /**
     *  @brief The name to use when registering the dbus service on the bus.
     *
     *
     */
    virtual std::string dbusServiceName() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief The dbus object path under which the interfaces will be
     *  registered.
     *
     *
     */
    virtual std::string dbusObjectPath() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Should return the path to a directory used to store temporary
     *  data like runc bundles.
     *
     *  This should be non-persistent storage and will be used for transient
     *  data.  If the directory doesn't exist the library will try and create
     *  it.  If the directory (and any leading dirs) has to be created it will
     *  be created with 1755 permissions.
     *
     *  Because of the way container setup works, this directory needs to be
     *  accessible - but not writable - by un-privileged processes.
     *
     */
    virtual std::string workspaceDir() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Should return a path to a directory on a persistent storage mount.
     *
     *  This is currently not used, but maybe in the future.
     *
     */
    virtual std::string persistentDir() const = 0;


    // -------------------------------------------------------------------------
    /**
     *  @brief A list of extra environment variables that will be set for all
     *  containers.
     *
     *  This would typically define platform specific variables.
     *
     */
    virtual std::map<std::string, std::string> extraEnvVariables() const = 0;

    // -------------------------------------------------------------------------
    /**
     * @brief Location to create the socket used for capturing container logs
     *
     * This needs to be somewhere writable
     */
    virtual std::string consoleSocketPath() const = 0;

public:
    // -------------------------------------------------------------------------
    /**
     *  Describes the details of any extra mounts needed to use the GPU. For
     *  example the bind mount for the nexus socket.
     *
     */
    struct GpuExtraMount
    {
        std::string source;
        std::string target;
        std::string type;
        std::set<std::string> flags;
    };

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns a list of extra device nodes that need to be mapped into
     *  the container to allow the apps to use the GPU.
     *
     *
     *
     */
    virtual std::list<std::string> gpuDeviceNodes() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the group id that the app needs to be in to access the
     *  GPU device nodes.
     *
     *  If no special gid for the GPU then this returns -1.  If there is a gid
     *  for the GPU dev nodes then the containered app will be in that
     *  supplementary group.
     *
     */
    virtual int gpuGroupId() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the details of any additional mounts required to access
     *  the GPU.
     *
     *  For example this is used on nexus platforms to map in the nexus server
     *  socket.
     *
     */
    virtual bool gpuHasExtraMounts() const = 0;
    virtual std::list<GpuExtraMount> gpuExtraMounts() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the set of external interface that container traffic
     *  maybe routed through.
     *
     *  On every RDK platform this is { "eth0", "wlan0" } but it may change.
     *
     */
    virtual std::set<std::string> externalInterfaces() const = 0;

};

#endif // !defined(IDOBBYSETTINGS_H)