#pragma once

#include <gmock/gmock.h>

#include "DobbySpecConfig.h"

class DobbySpecConfigMock : public DobbySpecConfig {

public:

    virtual ~DobbySpecConfigMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD((const std::map<std::string, Json::Value>&), rdkPlugins, (), (const));
    MOCK_METHOD(const std::string ,spec, (), (const));
    MOCK_METHOD(bool ,isValid, (), (const));
    MOCK_METHOD(std::shared_ptr<rt_dobby_schema> ,config, (), (const));
    MOCK_METHOD(bool ,restartOnCrash, (), (const));
};
