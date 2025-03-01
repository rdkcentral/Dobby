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

#pragma once

#include <gmock/gmock.h>
#include "DobbyRdkPluginManager.h"

class DobbyRdkPluginManagerMock : public DobbyRdkPluginManagerImpl {

public:

    virtual ~DobbyRdkPluginManagerMock() = default;
    MOCK_METHOD(void, setExitStatus, (int status), (override));
    MOCK_METHOD(const std::vector<std::string>, listLoadedPlugins, (), (const,override));
    MOCK_METHOD(std::shared_ptr<IDobbyRdkLoggingPlugin>, getContainerLogger, (), (const,override));
    MOCK_METHOD(bool, runPlugins,(const IDobbyRdkPlugin::HintFlags &hookPoint,const uint timeoutMs ),( const,override));
    MOCK_METHOD(bool, runPlugins,(const IDobbyRdkPlugin::HintFlags &hookPoint),( const,override));
    MOCK_METHOD(std::shared_ptr<DobbyRdkPluginUtils>,  getUtils, (), (const, override));
};

