
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
#include "DobbyLegacyPluginManager.h"

class DobbyLegacyPluginManagerMock : public DobbyLegacyPluginManagerImpl {

public:

    virtual ~DobbyLegacyPluginManagerMock() = default;
    MOCK_METHOD(void, refreshPlugins, (const std::string& path), (override));
    MOCK_METHOD(bool, executePostConstructionHooks, ((const std::map<std::string, Json::Value>& plugins),
                                      const ContainerId& id,
                                      const std::shared_ptr<IDobbyStartState>& startupState,
                                      const std::string& rootfsPath), (const, override));
    MOCK_METHOD(bool, executePreStartHooks, ((const std::map<std::string, Json::Value>& plugins),
                              const ContainerId& id,
                              pid_t pid,
                              const std::string& rootfsPath), (const, override));
    MOCK_METHOD(bool, executePostStartHooks, ((const std::map<std::string, Json::Value>& plugins),
                              const ContainerId& id,
                              pid_t pid,
                              const std::string& rootfsPath), (const, override));
    MOCK_METHOD(bool, executePostStopHooks, ((const std::map<std::string, Json::Value>& plugins),
                              const ContainerId& id,
                              const std::string& rootfsPath), (const, override));
    MOCK_METHOD(bool, executePreDestructionHooks,( (const std::map<std::string, Json::Value>& plugins),
                              const ContainerId& id,
                              const std::string& rootfsPath), (const, override));

};

