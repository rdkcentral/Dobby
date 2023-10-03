#pragma once

#include "IDobbyIPCUtils.h"
#include <gmock/gmock.h>

class DobbyIPCUtilsMock : public IDobbyIPCUtils {

public:

    DobbyIPCUtilsMock(const std::string& systemDbusAddress,
                      const std::shared_ptr<AI_IPC::IIpcService>& systemIpcService)
        : mSystemDbusAddress(systemDbusAddress), mSystemIpcService(systemIpcService) {}

    virtual ~DobbyIPCUtilsMock() = default;

    MOCK_METHOD(bool, setAIDbusAddress, (bool privateBus, const std::string& address), ());

private:
    const std::string mSystemDbusAddress;
    const std::shared_ptr<AI_IPC::IIpcService> mSystemIpcService;
};
