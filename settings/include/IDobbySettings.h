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
 * File:   IDobbySettings.h
 *
 */
#ifndef IDOBBYSETTINGS_H
#define IDOBBYSETTINGS_H

#include <memory>
#include <string>
#include <list>
#include <vector>
#include <map>
#include <set>
#include <netinet/in.h>

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

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
     *  Describes the details of any extra mounts needed to use the GPU or VPU.
     *  For example on broadcom we bind mount the nexus socket.
     *
     */
    struct ExtraMount
    {
        std::string source;
        std::string target;
        std::string type;
        std::set<std::string> flags;
    };

    // -------------------------------------------------------------------------
    /**
     *  Describes the details of anything extra needed to enable access to
     *  certain hardware blocks, like the GPU or VPU.
     *
     *      - deviceNodes
     *          List of extra device nodes that need to be mapped into
     *          the container to allow the apps to use the H/W.
     *      - groupIds
     *          The group id that the app needs to be in to access the
     *          H/W device nodes. If not empty then the containered app will be
     *          in that supplementary group(s).
     *      - extraMounts
     *          The details of any additional mounts required to access
     *          the H/W. For example this is used on nexus platforms to map in
     *          the nexus server socket.  This can also be used to map in
     *          extra files / sockets used by the software.
     *      - extraEnvVariables
     *          A list of extra environment variables that will be set for all
     *          containers if the given H/W access is requested.
     *
     */
    struct HardwareAccessSettings
    {
        std::list<std::string> deviceNodes;
        std::set<int> groupIds;
        std::list<ExtraMount> extraMounts;
        std::map<std::string, std::string> extraEnvVariables;
    };

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns any extra details needed to access the GPU inside the
     *  container.
     *
     */
    virtual std::shared_ptr<HardwareAccessSettings> gpuAccessSettings() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns any extra details needed to access the VPU (video
     *  pipeline) inside the container.
     *
     */
    virtual std::shared_ptr<HardwareAccessSettings> vpuAccessSettings() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the set of external interface that container traffic
     *  maybe routed through.
     *
     *  On every RDK platform this is { "eth0", "wlan0" } but it may change.
     *
     */
    virtual std::vector<std::string> externalInterfaces() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the Dobby network address range in string format
     *
     *  IPv4 address, masked with /24, i.e. address can be nnn.nnn.nnn.0
     *
     */
    virtual std::string addressRangeStr() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the Dobby network address range in in_addr_t format
     *
     *  IPv4 address, masked with /24, i.e. address can be nnn.nnn.nnn.0
     *
     */
    virtual in_addr_t addressRange() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns any default plugins the platform should run
     *
     *  It's assumed the plugins will have an empty data section (i.e. {})
     *  and that the default plugin options will always be suitable
     *
     */
    virtual std::vector<std::string> defaultPlugins() const = 0;

    virtual Json::Value rdkPluginsData() const = 0;

    struct LogRelaySettings
    {
        bool syslogEnabled;
        bool journaldEnabled;

        std::string syslogSocketPath;
        std::string journaldSocketPath;
    };

    virtual LogRelaySettings logRelaySettings() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  Settings needed for running an app with strace
     *
     *      - logsDir
     *          Path to directory where strace logs will be written
     *      - apps
     *          A list of app names that should be run with strace.
     *          Hostname field from containers config is used as app name.
     *
     */
    struct StraceSettings
    {
        std::string logsDir;
        std::vector<std::string> apps;
    };

    virtual StraceSettings straceSettings() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  Apparmor settings
     *
     *      - enabled
     *          Specifies if apparmor profile should be set for containered apps
     *      - profileName
     *          A name of default apparmor profile used for containered apps
     *
     */
    struct ApparmorSettings
    {
        bool enabled;
        std::string profileName;
    };

    virtual ApparmorSettings apparmorSettings() const = 0;
};

#endif // !defined(IDOBBYSETTINGS_H)