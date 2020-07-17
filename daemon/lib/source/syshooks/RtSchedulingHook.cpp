/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * File:   RtSchedulingHook.cpp
 *
 * Copyright (C) Sky UK 2016+
 */
#include "RtSchedulingHook.h"

#include <Logging.h>

#include <sched.h>

RtSchedulingHook::RtSchedulingHook()
{
}

RtSchedulingHook::~RtSchedulingHook()
{
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the name of the hook
 *
 */
std::string RtSchedulingHook::hookName() const
{
    return std::string("RtSchedHook");
}

// -----------------------------------------------------------------------------
/**
 *  @brief Hook hints for when to run the network hook
 *
 *  We only need to be called at the pre-start phase.
 *
 */
unsigned RtSchedulingHook::hookHints() const
{
    return IDobbySysHook::PreStartSync;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the RT scheduling priority on the container's init process
 *
 *  This simply calls sched_setscheduler on the supplied pid.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  containerPid    The pid of the process in the container.
 *  @param[in]  config          The container config.
 *  @param[in]  rootfs          The path to the container rootfs.
 *
 *  @return true if successiful otherwise false.
 */
bool RtSchedulingHook::preStart(const ContainerId& id,
                                pid_t containerPid,
                                const std::shared_ptr<const DobbyConfig>& config,
                                const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    AI_LOG_FN_ENTRY();

    struct sched_param schedParam;
    schedParam.sched_priority = config->rtPriorityDefault();

    if (sched_setscheduler(containerPid, SCHED_RR, &schedParam) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to set RR scheduling policy for "
                              "container '%s'", id.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}
