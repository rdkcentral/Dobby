/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#pragma once

#include "gmock/gmock.h"
#include "IDobbySettings.h"

class DobbySettingsMock : public IDobbySettings {
    public:
    virtual ~DobbySettingsMock() = default;

    MOCK_METHOD(std::string, workspaceDir, (), (const, override));
    MOCK_METHOD(std::string, persistentDir, (), (const, override));
    MOCK_METHOD((std::map<std::string, std::string>), extraEnvVariables, (), (const, override));
    MOCK_METHOD(std::string, consoleSocketPath, (), (const, override));
    MOCK_METHOD(std::shared_ptr<HardwareAccessSettings>, gpuAccessSettings, (), (const, override));
    MOCK_METHOD(std::shared_ptr<HardwareAccessSettings>, vpuAccessSettings, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, externalInterfaces, (), (const, override));
    MOCK_METHOD(std::string, addressRangeStr, (), (const, override));
    MOCK_METHOD(in_addr_t, addressRange, (), (const, override));
    MOCK_METHOD(std::vector<std::string>, defaultPlugins, (), (const, override));
    MOCK_METHOD(Json::Value, rdkPluginsData, (), (const, override));
    MOCK_METHOD(LogRelaySettings, logRelaySettings, (), (const, override));
    MOCK_METHOD(StraceSettings, straceSettings, (), (const, override));
    MOCK_METHOD(ApparmorSettings, apparmorSettings, (), (const, override));
    MOCK_METHOD(PidsSettings, pidsSettings, (), (const, override));
};
