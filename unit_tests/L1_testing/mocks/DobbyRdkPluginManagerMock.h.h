#pragma once

#include <gmock/gmock.h>

#include "DobbyRdkPluginManager.h"

class DobbyRdkPluginManagerMock : public DobbyRdkPluginManager {

public:

    virtual ~DobbyRdkPluginManagerMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(const std::vector<std::string>, listLoadedPlugins, (), (const));
    MOCK_METHOD(std::shared_ptr<IDobbyRdkLoggingPlugin>, getContainerLogger, (), (const));
};

