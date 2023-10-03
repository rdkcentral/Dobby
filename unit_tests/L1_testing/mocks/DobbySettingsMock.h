#pragma once

#include "gmock/gmock.h"
#include "IDobbySettings.h"

class DobbySettingsMock : public IDobbySettings {
    public:
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
};
