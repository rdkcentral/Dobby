#pragma once

#include <gmock/gmock.h>

#include "IpcFileDescriptor.h"

class IpcFileDescriptorMock : public AI_IPC::IpcFileDescriptor {

public:

    virtual ~IpcFileDescriptorMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(void, reset, (int fd_), ());

};



