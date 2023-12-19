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

#include "DobbyRdkPluginManagerMock.h"

DobbyRdkPluginManager::DobbyRdkPluginManager()
{
}

DobbyRdkPluginManager::DobbyRdkPluginManager(std::shared_ptr<rt_dobby_schema> containerConfig,const std::string &rootfsPath,const std::string &pluginPath,const std::shared_ptr<DobbyRdkPluginUtils> &utils)
{
}

DobbyRdkPluginManager::~DobbyRdkPluginManager()
{
}

void DobbyRdkPluginManager::setImpl(DobbyRdkPluginManagerImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

bool DobbyRdkPluginManager::runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs ) const
{
   EXPECT_NE(impl, nullptr);

    return impl->runPlugins(hookPoint,timeoutMs);
}

bool DobbyRdkPluginManager::runPlugins(const IDobbyRdkPlugin::HintFlags &hookPoint) const
{
   EXPECT_NE(impl, nullptr);

    return impl->runPlugins(hookPoint);
}

void DobbyRdkPluginManager::setExitStatus(int status) const
{
   EXPECT_NE(impl, nullptr);

    return impl->setExitStatus(status);
}

const std::vector<std::string> DobbyRdkPluginManager::listLoadedPlugins() const
{
   EXPECT_NE(impl, nullptr);

    return impl->listLoadedPlugins();
}

std::shared_ptr<IDobbyRdkLoggingPlugin> DobbyRdkPluginManager::getContainerLogger() const
{
   EXPECT_NE(impl, nullptr);

    return impl->getContainerLogger();
}

