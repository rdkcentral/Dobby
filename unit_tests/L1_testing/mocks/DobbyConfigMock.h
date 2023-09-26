#pragma once

#include <gmock/gmock.h>

#include "DobbyConfig.h"

class DobbyConfigMock : public DobbyConfig {
public:
    virtual ~DobbyConfigMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(bool, writeConfigJson, (const std::string& filePath), (const));
};