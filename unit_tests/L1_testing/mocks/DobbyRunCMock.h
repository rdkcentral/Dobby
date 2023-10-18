#pragma once

#include <gmock/gmock.h>

#include "DobbyRunC.h"

class DobbyRunCMock : public DobbyRunC {

public:

    virtual ~DobbyRunCMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
MOCK_METHOD(bool, killCont, (const ContainerId &id, int signal, bool all), (const));
MOCK_METHOD(bool, resume, (const ContainerId& id), (const));
MOCK_METHOD(bool, pause, (const ContainerId& id), (const));
MOCK_METHOD((std::pair<pid_t, pid_t>), exec, (const ContainerId& id, const std::string& options, const std::string& command), (const));
};

