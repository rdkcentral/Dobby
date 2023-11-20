
/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "DobbyContainerMock.h"

DobbyContainer::DobbyContainer(): descriptor(0)
{
}

DobbyContainer::DobbyContainer(const std::shared_ptr<const DobbyBundle>& _bundle,const std::shared_ptr<const DobbyConfig>& _config,const std::shared_ptr<const DobbyRootfs>& _rootfs):descriptor(0)
{
}

DobbyContainer::DobbyContainer(const std::shared_ptr<const DobbyBundle>& _bundle,
                               const std::shared_ptr<const DobbyConfig>& _config,
                               const std::shared_ptr<const DobbyRootfs>& _rootfs,
                               const std::shared_ptr<const DobbyRdkPluginManager>& _rdkPluginManager)
    : descriptor(0)
    , bundle(_bundle)
    , config(_config)
    , rootfs(_rootfs)
    , rdkPluginManager(_rdkPluginManager)
    , containerPid(-1)
    , hasCurseOfDeath(false)
    , state(State::Starting)
{
}

DobbyContainer::~DobbyContainer()
{
}

bool DobbyContainer::shouldRestart(int statusCode)
{
   EXPECT_NE(impl, nullptr);

    return impl->shouldRestart(statusCode);
}

void DobbyContainer::setImpl(DobbyContainerImpl* newImpl)
{
     impl = newImpl;
}

DobbyContainer* DobbyContainer::getInstance()
{
    static DobbyContainer* instance = nullptr;
    if (nullptr == instance)
    {
        instance = new DobbyContainer();
    }
    return instance;
}

void DobbyContainer::setRestartOnCrash(const std::list<int>& files)
{
   EXPECT_NE(impl, nullptr);

    return impl->setRestartOnCrash(files);
}

void DobbyContainer::clearRestartOnCrash()
{
   EXPECT_NE(impl, nullptr);

    return impl->clearRestartOnCrash();
}

const std::list<int>& DobbyContainer::files()
{
   EXPECT_NE(impl, nullptr);

    return impl->files();
}

