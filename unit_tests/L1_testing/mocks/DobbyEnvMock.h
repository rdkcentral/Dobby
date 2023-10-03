#pragma once

#include "DobbyEnv.h"
#include <gmock/gmock.h>

class DobbyEnvMock : public IDobbyEnv {

public:

    DobbyEnvMock(const std::shared_ptr<const IDobbySettings>& settings)
        : mSettings(settings) {}

    virtual ~DobbyEnvMock() = default;

    MOCK_METHOD(std::string, workspaceMountPath, (), (const));
    MOCK_METHOD(std::string, flashMountPath, (), (const));
    MOCK_METHOD(std::string, pluginsWorkspacePath, (), (const));
    MOCK_METHOD(std::string, cgroupMountPath, (Cgroup cgroup), (const));
    MOCK_METHOD(uint16_t, platformIdent, (), (const));

private:
    const std::shared_ptr<const IDobbySettings> mSettings;
};
