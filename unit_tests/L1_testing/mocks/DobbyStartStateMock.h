#pragma once

#include <gmock/gmock.h>

#include "DobbyStartState.h"

class DobbyStartStateMock {

public:

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(std::list<int>, files, (), (const));
    MOCK_METHOD(bool, isValid, (), (const));

};