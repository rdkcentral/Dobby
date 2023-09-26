#pragma once

#include <gmock/gmock.h>

#include "DobbyBundle.h"

class DobbyBundleMock : public DobbyBundle {

public:

    virtual ~DobbyBundleMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(void, setPersistence, (bool persist), ());
    MOCK_METHOD(bool, isValid, (), (const));


};
