#pragma once

#include "DobbyManager.h"
#include <gmock/gmock.h>

class  DobbyManagerMock : public DobbyManager {

public:

    MOCK_METHOD(int32_t, startContainerFromSpec, (const ContainerId& id,
                                              const std::string& jsonSpec,
                                              const std::list<int>& files,
                                              const std::string& command,
                                              const std::string& displaySocket,
                                              const std::vector<std::string>& envVars),(const));

    MOCK_METHOD(int32_t, startContainerFromBundle, (const ContainerId& id,
                                                const std::string& bundlePath,
                                                const std::list<int>& files,
                                                const std::string& command,
                                                const std::string& displaySocket,
                                                const std::vector<std::string>& envVars), (const));

    MOCK_METHOD(bool, stopContainer, (int32_t cd, bool withPrejudice), ());

    MOCK_METHOD(bool, pauseContainer, (int32_t cd), ());

    MOCK_METHOD(bool, resumeContainer, (int32_t cd), ());

    MOCK_METHOD(bool, execInContainer, (int32_t cd,
                                   const std::string& options,
                                   const std::string& command), ());

    MOCK_METHOD((std::list<std::pair<int32_t, ContainerId>>), listContainers, (), (const));

    MOCK_METHOD(int32_t, stateOfContainer, (int32_t cd), (const));

    MOCK_METHOD(std::string, statsOfContainer, (int32_t cd), (const));

    MOCK_METHOD(std::string, ociConfigOfContainer, (int32_t cd), (const));

    MOCK_METHOD(std::string, specOfContainer, (int32_t cd), (const));

    MOCK_METHOD(bool, createBundle, (const ContainerId& id, const std::string& jsonSpec), ());

};
