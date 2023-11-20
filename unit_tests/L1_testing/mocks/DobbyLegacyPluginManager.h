/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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
 * File:   DobbyLegacyPluginManager.h
 *
 */

#ifndef DOBBYLEGACYPLUGINMANAGER_H
#define DOBBYLEGACYPLUGINMANAGER_H

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

#include <pthread.h>
#include <sys/types.h>

#include <map>
#include <string>
#include <memory>
#include <future>

#if defined(RDK)
#   define DEFAULT_PLUGIN_PATH   "/usr/lib/plugins/dobby"
#else
#   define DEFAULT_PLUGIN_PATH   "/opt/libexec"
#endif

class ContainerId;
class IDobbyPlugin;
class IDobbyStartState;
class IDobbyEnv;

// -----------------------------------------------------------------------------
class DobbyLegacyPluginManagerImpl
{
public:
    virtual ~DobbyLegacyPluginManagerImpl() = default;

    virtual void refreshPlugins(const std::string& path = std::string(DEFAULT_PLUGIN_PATH)) = 0;

    virtual bool executePostConstructionHooks(const std::map<std::string, Json::Value>& plugins,
                                      const ContainerId& id,
                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                      const std::string& rootfsPath) const = 0;

    virtual bool executePreStartHooks(const std::map<std::string, Json::Value>& plugins,
                              const ContainerId& id,
                              pid_t pid,
                              const std::string& rootfsPath) const = 0;

    virtual bool executePostStartHooks(const std::map<std::string, Json::Value>& plugins,
                               const ContainerId& id,
                               pid_t pid,
                               const std::string& rootfsPath) const = 0;

    virtual bool executePostStopHooks(const std::map<std::string, Json::Value>& plugins,
                              const ContainerId& id,
                              const std::string& rootfsPath) const = 0;

    virtual bool executePreDestructionHooks(const std::map<std::string, Json::Value>& plugins,
                                    const ContainerId& id,
                                    const std::string& rootfsPath) const = 0;
};
/**
 *  @class DobbyLegacyPluginManager
 *  @brief Class that manages all the plugin hook libraries.
 *
 *  This class doesn't manage the system hooks, they are setup in the
 *  DobbyManager class (we should probably change this ... TBD)
 *
 *  At creation time it loads all the plugin libraries from /opt/libexec.
 *
 */
class DobbyLegacyPluginManager
{
protected:
    static DobbyLegacyPluginManagerImpl *impl;
public:
    DobbyLegacyPluginManager();
    DobbyLegacyPluginManager(const std::shared_ptr<IDobbyEnv>& env,
                             const std::shared_ptr<IDobbyUtils>& utils,
                             const std::string& path = std::string(DEFAULT_PLUGIN_PATH));
    ~DobbyLegacyPluginManager();

public:
    static DobbyLegacyPluginManager* getInstance();
    static void setImpl(DobbyLegacyPluginManagerImpl* newImpl);
    static void refreshPlugins(const std::string& path = std::string(DEFAULT_PLUGIN_PATH));
    static bool executePostConstructionHooks(const std::map<std::string, Json::Value>& plugins,
                                      const ContainerId& id,
                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                      const std::string& rootfsPath);
    static bool executePreStartHooks(const std::map<std::string, Json::Value>& plugins,
                              const ContainerId& id,
                              pid_t pid,
                              const std::string& rootfsPath);
    static bool executePostStartHooks(const std::map<std::string, Json::Value>& plugins,
                               const ContainerId& id,
                               pid_t pid,
                               const std::string& rootfsPath);
    static bool executePostStopHooks(const std::map<std::string, Json::Value>& plugins,
                              const ContainerId& id,
                              const std::string& rootfsPath);
    static bool executePreDestructionHooks(const std::map<std::string, Json::Value>& plugins,
                                    const ContainerId& id,
                                    const std::string& rootfsPath);
};


#endif // !defined(DOBBYLEGACYPLUGINMANAGER_H)
