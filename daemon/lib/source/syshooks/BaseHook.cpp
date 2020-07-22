/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   BaseHook.cpp
 *
 */
#include "BaseHook.h"



BaseHook::~BaseHook()
{
}

unsigned BaseHook::hookHints() const
{
    return 0;
}

bool BaseHook::postConstruction(const ContainerId& id,
                                const std::shared_ptr<IDobbyStartState>& startupState,
                                const std::shared_ptr<const DobbyConfig>& config,
                                const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    return true;
}

bool BaseHook::preStart(const ContainerId& id,
                        pid_t containerPid,
                        const std::shared_ptr<const DobbyConfig>& config,
                        const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    return true;
}

bool BaseHook::postStart(const ContainerId& id,
                         pid_t containerPid,
                         const std::shared_ptr<const DobbyConfig>& config,
                         const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    return true;
}

bool BaseHook::postStop(const ContainerId& id,
                        const std::shared_ptr<const DobbyConfig>& config,
                        const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    return true;
}

bool BaseHook::preDestruction(const ContainerId& id,
                              const std::shared_ptr<const DobbyConfig>& config,
                              const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    return true;
}
