#pragma once

#include "DobbyWorkQueue.h"
#include <gmock/gmock.h>

class DobbyWorkQueueMock : public DobbyWorkQueue{

public:

    virtual ~DobbyWorkQueueMock() = default;

    MOCK_METHOD(bool, runFor, (const std::chrono::milliseconds& msecs), (const));
    MOCK_METHOD(bool, postWork, (WorkFunc&& work), (const));
    MOCK_METHOD(void, exit, (), ());
};
