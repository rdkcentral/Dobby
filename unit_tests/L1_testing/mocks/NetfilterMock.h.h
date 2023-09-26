#pragma once

#include <gmock/gmock.h>

#include "Netfilter.h"

class NetfilterMock : public Netfilter {
public:
    virtual ~NetfilterMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(bool, writeString, (int fd, const std::string &str), (const));
};


