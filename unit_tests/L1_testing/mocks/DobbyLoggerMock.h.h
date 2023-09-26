#pragma once

#include <gmock/gmock.h>

#include "DobbyLogger.h"

class DobbyLoggerMock : public DobbyLogger {

public:

    virtual ~DobbyLoggerMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(bool, StartContainerLogging, (std::string containerId,
                               pid_t runtimePid,
                               pid_t containerPid,
                               std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin), ());
};

