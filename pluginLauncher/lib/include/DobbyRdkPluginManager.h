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
 * File:   DobbyRdkPluginManager.h
 *
 */
#ifndef DOBBYRDKPLUGINMANAGER_H
#define DOBBYRDKPLUGINMANAGER_H

#include "IDobbyRdkPlugin.h"
#include "IDobbyRdkLoggingPlugin.h"

#include <sys/types.h>
#include <map>
#include <string>
#include <memory>
#include <set>
#include <vector>
#include <algorithm>

class DobbyRdkPluginDependencySolver;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyRdkPluginManager
 *  @brief Class that manages all the RDK plugin hook libraries.
 *
 *  At creation time it loads all the plugin libraries.
 *
 */
class DobbyRdkPluginManager
{
public:
    DobbyRdkPluginManager(std::shared_ptr<rt_dobby_schema> containerConfig,
                          const std::string &rootfsPath,
                          const std::string &pluginPath,
                          const std::shared_ptr<DobbyRdkPluginUtils> &utils);
    ~DobbyRdkPluginManager();

public:
    const std::vector<std::string> listLoadedPlugins() const;
    const std::vector<std::string> listLoadedLoggers() const;
    bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint,
                    const uint timeoutMs = 0) const;

    // This is public as RDKPluginManager isn't responsible for handling logging
    std::shared_ptr<IDobbyRdkLoggingPlugin> getContainerLogger() const;
    void setExitStatus(int status) const;

private:
    bool loadPlugins();
    bool preprocessPlugins();
    bool executeHook(const std::string &pluginName,
                     const IDobbyRdkPlugin::HintFlags hook) const;
    bool executeHookTimeout(const std::string &pluginName,
                            const IDobbyRdkPlugin::HintFlags hook,
                            const uint timeoutMs) const;
    std::string HookPointToString(const IDobbyRdkPlugin::HintFlags &hookPoint) const;

    bool implementsHook(const std::string &pluginName,
                        const IDobbyRdkPlugin::HintFlags hook) const;
    bool isLoaded(const std::string &pluginName) const;
    bool isRequired(const std::string &pluginName) const;
    inline std::shared_ptr<IDobbyRdkPlugin> getPlugin(const std::string &name) const;
    inline std::shared_ptr<IDobbyRdkLoggingPlugin> getLogger(const std::string &name) const;

private:
    bool mValid;
    std::map<std::string, std::pair<void *, std::shared_ptr<IDobbyRdkLoggingPlugin>>> mLoggers;
    std::map<std::string, std::pair<void *, std::shared_ptr<IDobbyRdkPlugin>>> mPlugins;
    std::set<std::string> mRequiredPlugins;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::string mPluginPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    std::unique_ptr<DobbyRdkPluginDependencySolver> mDependencySolver;
};

#endif // !defined(DOBBYRDKPLUGINMANAGER_H)
