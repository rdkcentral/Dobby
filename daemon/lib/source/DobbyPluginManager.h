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
 * File:   DobbyPluginManager.h
 *
 */
#ifndef DOBBYPLUGINMANAGER_H
#define DOBBYPLUGINMANAGER_H

#include "IDobbyUtils.h"
#include "ContainerId.h"

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



#if defined(DEV_VM)
#   define DEFAULT_PLUGIN_PATH   "/usr/local/lib/plugins/dobby"
#elif defined(RDK)
#   define DEFAULT_PLUGIN_PATH   "/usr/lib/plugins/dobby"
#else
#   define DEFAULT_PLUGIN_PATH   "/opt/libexec"
#endif

class IDobbyPlugin;
class IDobbyStartState;
class IDobbyEnv;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyPluginManager
 *  @brief Class that manages all the plugin hook libraries.
 *
 *  This class doesn't manage the system hooks, they are setup in the
 *  DobbyManager class (we should probably change this ... TBD)
 *
 *  At creation time it loads all the plugin libraries from /opt/libexec.
 *
 */
class DobbyPluginManager
{
public:
    DobbyPluginManager(const std::shared_ptr<IDobbyEnv>& env,
                       const std::shared_ptr<IDobbyUtils>& utils,
                       const std::string& path = std::string(DEFAULT_PLUGIN_PATH));
    ~DobbyPluginManager();

public:
    void refreshPlugins(const std::string& path = std::string(DEFAULT_PLUGIN_PATH));

public:
    bool executePostConstructionHooks(const std::map<std::string, Json::Value>& plugins,
                                      const ContainerId& id,
                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                      const std::string& rootfsPath) const;

    bool executePreStartHooks(const std::map<std::string, Json::Value>& plugins,
                              const ContainerId& id,
                              pid_t pid,
                              const std::string& rootfsPath) const;

    bool executePostStartHooks(const std::map<std::string, Json::Value>& plugins,
                               const ContainerId& id,
                               pid_t pid,
                               const std::string& rootfsPath) const;

    bool executePostStopHooks(const std::map<std::string, Json::Value>& plugins,
                              const ContainerId& id,
                              const std::string& rootfsPath) const;

    bool executePreDestructionHooks(const std::map<std::string, Json::Value>& plugins,
                                    const ContainerId& id,
                                    const std::string& rootfsPath) const;

private:
    typedef std::function<bool (IDobbyPlugin*, const Json::Value&)> HookFn;

    bool executeHooks(const std::map<std::string, Json::Value>& plugins,
                      const HookFn& hookFn,
                      unsigned asyncFlag,
                      unsigned syncFlag) const;


private:
    inline std::shared_ptr<IDobbyPlugin> getPlugin(const std::string& name) const;
    void loadPlugins(const std::string& path);

private:
    mutable pthread_rwlock_t mRwLock;
    const std::shared_ptr<IDobbyEnv> mEnvironment;
    const std::shared_ptr<IDobbyUtils> mUtilities;
    std::map<std::string, std::pair<void*, std::shared_ptr<IDobbyPlugin>>> mPlugins;

};


#endif // !defined(DOBBYPLUGINMANAGER_H)
