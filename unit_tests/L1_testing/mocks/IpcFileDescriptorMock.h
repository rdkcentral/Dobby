#pragma once

#include <gmock/gmock.h>
#include "IpcFileDescriptor.h"

class IpcFileDescriptorMock : public AI_IPC::IpcFileDescriptor {

public:

    virtual ~IpcFileDescriptorMock() = default;

    MOCK_METHOD(bool, isValid, (), (const));
    MOCK_METHOD(int, fd, (), (const));
    MOCK_METHOD(void, reset, (int fd_), ());

};



