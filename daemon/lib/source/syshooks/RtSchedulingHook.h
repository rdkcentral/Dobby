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
 * File:   RtSchedulingHook.h
 *
 */
#ifndef RTSCHEDULINGHOOK_H
#define RTSCHEDULINGHOOK_H

#include "BaseHook.h"

#include <memory>

// -----------------------------------------------------------------------------
/**
 *  @class RtSchedulingHook
 *  @brief Basic hook that just sets the RT priority of the container.
 *
 *  This consists of just a pre-start hook that calls sched_setscheduler on
 *  the containers init process.
 *
 *  Due to the way scheduling is inherited this will in turn filter down to
 *  all processes running within the container.
 *
 */
class RtSchedulingHook : public BaseHook
{
public:
    RtSchedulingHook();
    ~RtSchedulingHook() final;

public:
    std::string hookName() const override;

    unsigned hookHints() const override;

    bool preStart(const ContainerId& id,
                  pid_t containerPid,
                  const std::shared_ptr<const DobbyConfig>& config,
                  const std::shared_ptr<const DobbyRootfs>& rootfs) override;

};


#endif // !defined(RTSCHEDULINGHOOK_H)
