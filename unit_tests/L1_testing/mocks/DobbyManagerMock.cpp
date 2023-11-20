/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "DobbyEnv.h"
#include "DobbyUtils.h"
#include "DobbyIPCUtils.h"
#include "DobbyManagerMock.h"

DobbyManager::DobbyManager()
{
}

DobbyManager::DobbyManager(std::shared_ptr<DobbyEnv>&, std::shared_ptr<DobbyUtils>&, std::shared_ptr<DobbyIPCUtils>&, const std::shared_ptr<const IDobbySettings>&, std::function<void(int, const ContainerId&)>&, std::function<void(int, const ContainerId&, int)>&)
{
}

DobbyManager::~DobbyManager()
{
}

void DobbyManager::setImpl(DobbyManagerImpl* newImpl)
{
    impl = newImpl;
}

DobbyManager* DobbyManager::getInstance()
{
    static DobbyManager* instance = nullptr;
    if(nullptr == instance)
    {
        instance = new DobbyManager();
    }
    return instance;
}

#if defined(LEGACY_COMPONENTS)

int32_t DobbyManager::startContainerFromSpec(const ContainerId& id,
                                      const std::string& jsonSpec,
                                      const std::list<int>& files,
                                      const std::string& command,
                                      const std::string& displaySocket,
                                      const std::vector<std::string>& envVars)
{
   EXPECT_NE(impl, nullptr);

    return impl->startContainerFromSpec(id, jsonSpec, files, command, displaySocket, envVars);
}

std::string DobbyManager::specOfContainer(int32_t cd)
{
   EXPECT_NE(impl, nullptr);

    return impl->specOfContainer(cd);
}

bool DobbyManager::createBundle(const ContainerId& id, const std::string& jsonSpec)
{
   EXPECT_NE(impl, nullptr);

    return impl->createBundle(id, jsonSpec);
}
#endif //defined(LEGACY_COMPONENTS)

int32_t DobbyManager::startContainerFromBundle(const ContainerId& id,
                                        const std::string& bundlePath,
                                        const std::list<int>& files,
                                        const std::string& command,
                                        const std::string& displaySocket,
                                        const std::vector<std::string>& envVars)
{
   EXPECT_NE(impl, nullptr);

    return impl->startContainerFromBundle(id, bundlePath, files, command, displaySocket, envVars);
}

bool DobbyManager::stopContainer(int32_t cd, bool withPrejudice)
{
   EXPECT_NE(impl, nullptr);

    return impl->stopContainer(cd, withPrejudice);
}

bool DobbyManager::pauseContainer(int32_t cd)
{
   EXPECT_NE(impl, nullptr);

    return impl->pauseContainer(cd);
}

bool DobbyManager::resumeContainer(int32_t cd)
{
   EXPECT_NE(impl, nullptr);

    return impl->resumeContainer(cd);
}

bool DobbyManager::execInContainer(int32_t cd,
                            const std::string& options,
                            const std::string& command)
{
   EXPECT_NE(impl, nullptr);

    return impl->execInContainer(cd, options, command);
}

std::list<std::pair<int32_t, ContainerId>> DobbyManager::listContainers()
{
   EXPECT_NE(impl, nullptr);

    return impl->listContainers();
}

int32_t DobbyManager::stateOfContainer(int32_t cd)
{
   EXPECT_NE(impl, nullptr);

    return impl->stateOfContainer(cd);
}

std::string DobbyManager::statsOfContainer(int32_t cd)
{
   EXPECT_NE(impl, nullptr);

    return impl->statsOfContainer(cd);
}

std::string DobbyManager::ociConfigOfContainer(int32_t cd)
{
   EXPECT_NE(impl, nullptr);

    return impl->ociConfigOfContainer(cd);
}


