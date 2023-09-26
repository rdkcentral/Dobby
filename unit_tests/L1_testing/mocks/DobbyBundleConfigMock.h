#pragma once

#include <gmock/gmock.h>

#include "DobbyBundleConfig.h"

class DobbyBundleConfigMock : public DobbyBundleConfig {

public:

    virtual ~DobbyBundleConfigMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(std::shared_ptr<rt_dobby_schema>, config, (), (const));
    MOCK_METHOD(bool, restartOnCrash, (), (const));

};

