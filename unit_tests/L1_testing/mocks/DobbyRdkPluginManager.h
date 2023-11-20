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

#ifndef DOBBYRDKPLUGINMANAGER_H
#define DOBBYRDKPLUGINMANAGER_H

#include <algorithm>
#include "IDobbyRdkPlugin.h"
#include "IDobbyRdkLoggingPlugin.h"

class DobbyRdkPluginUtils;

class DobbyRdkPluginManagerImpl {

public:

    virtual bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs ) const = 0;
    virtual bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint) const = 0;
    virtual void setExitStatus(int status) = 0;
    virtual const std::vector<std::string> listLoadedPlugins() const = 0;
    virtual std::shared_ptr<IDobbyRdkLoggingPlugin> getContainerLogger() const = 0;

};

class DobbyRdkPluginManager {

protected:
    static DobbyRdkPluginManagerImpl* impl;

public:

    DobbyRdkPluginManager();
    DobbyRdkPluginManager(std::shared_ptr<rt_dobby_schema> containerConfig,const std::string &rootfsPath,const std::string &pluginPath,const std::shared_ptr<DobbyRdkPluginUtils> &utils);
    ~DobbyRdkPluginManager();

    static void setImpl(DobbyRdkPluginManagerImpl* newImpl);
    static DobbyRdkPluginManager* getInstance();
    static bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs );
    static bool runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint);
    static void setExitStatus(int status);
    static const std::vector<std::string> listLoadedPlugins();
    static std::shared_ptr<IDobbyRdkLoggingPlugin> getContainerLogger();

};

#endif // !defined(DOBBYRDKPLUGINMANAGER_H)

