#pragma once

#include <gmock/gmock.h>

#include "DobbyContainer.h"

class DobbyContainerMock : public DobbyContainer {

public:

    virtual ~DobbyContainerMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(void, setRestartOnCrash, (const std::list<int>& files), ());
    MOCK_METHOD(void, clearRestartOnCrash, (), ());

};
