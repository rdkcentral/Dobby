#pragma once

#include <gmock/gmock.h>

#include "DobbyStats.h"

class DobbyStatsMock : public DobbyStats {

public:

    virtual ~DobbyStatsMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(const Json::Value&, stats, (), (const));
};

