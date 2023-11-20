
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

#include "IDobbyUtils.h"
#include "DobbyLegacyPluginManagerMock.h"

DobbyLegacyPluginManager::DobbyLegacyPluginManager()
{
}

DobbyLegacyPluginManager::DobbyLegacyPluginManager(const std::shared_ptr<IDobbyEnv>& env,
                                                   const std::shared_ptr<IDobbyUtils>& utils,
                                                   const std::string& path /*= std::string(DEFAULT_PLUGIN_PATH)*/)
{
}

DobbyLegacyPluginManager::~DobbyLegacyPluginManager()
{
}

void DobbyLegacyPluginManager::refreshPlugins(const std::string& path /*= std::string(DEFAULT_PLUGIN_PATH)*/)
{
   EXPECT_NE(impl, nullptr);

    return impl->refreshPlugins(path);
}

bool DobbyLegacyPluginManager::executePostConstructionHooks(const std::map<std::string, Json::Value>& plugins,
                                  const ContainerId& id,
                                  const std::shared_ptr<IDobbyStartState>& startupState,
                                  const std::string& rootfsPath)
{
   EXPECT_NE(impl, nullptr);

    return impl->executePostConstructionHooks(plugins,id,startupState,rootfsPath);
}

bool DobbyLegacyPluginManager::executePreStartHooks(const std::map<std::string, Json::Value>& plugins,
                          const ContainerId& id,
                          pid_t pid,
                          const std::string& rootfsPath)
{
   EXPECT_NE(impl, nullptr);

    return impl->executePreStartHooks(plugins,id,pid,rootfsPath);
}

bool DobbyLegacyPluginManager::executePostStartHooks(const std::map<std::string, Json::Value>& plugins,
                           const ContainerId& id,
                           pid_t pid,
                           const std::string& rootfsPath)
{
   EXPECT_NE(impl, nullptr);

    return impl->executePostStartHooks(plugins,id,pid,rootfsPath);
}

bool DobbyLegacyPluginManager::executePostStopHooks(const std::map<std::string, Json::Value>& plugins,
                          const ContainerId& id,
                          const std::string& rootfsPath)
{
   EXPECT_NE(impl, nullptr);

    return impl->executePostStopHooks(plugins,id,rootfsPath);
}

bool DobbyLegacyPluginManager::executePreDestructionHooks(const std::map<std::string, Json::Value>& plugins,
                                const ContainerId& id,
                                const std::string& rootfsPath)
{
   EXPECT_NE(impl, nullptr);

    return impl->executePreDestructionHooks(plugins,id,rootfsPath);
}

DobbyLegacyPluginManager* DobbyLegacyPluginManager::getInstance()
{
    static DobbyLegacyPluginManager* instance = nullptr;
    if (nullptr == instance)
    {
        instance = new DobbyLegacyPluginManager();
    }
    return instance;
}

void DobbyLegacyPluginManager::setImpl(DobbyLegacyPluginManagerImpl* newImpl)
{
    impl = newImpl;
}

