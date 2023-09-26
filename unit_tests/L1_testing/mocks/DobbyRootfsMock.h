#pragma once

#include <gmock/gmock.h>

#include "DobbyRootfs.h"

class DobbyRootfsMock : public DobbyRootfs {

public:

    virtual ~DobbyRootfsMock() = default;

    // Declare the mock method using the MOCK_METHOD macro
    MOCK_METHOD(void, setPersistence, (bool persist), ());
    MOCK_METHOD(const std::string&, path, (), (const));
    MOCK_METHOD(bool, isValid, (), (const));
};


