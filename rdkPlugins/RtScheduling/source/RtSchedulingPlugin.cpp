/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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

#include "RtSchedulingPlugin.h"

#include <Logging.h>

#include <errno.h>
#include <unistd.h>
#include <limits.h>

// default rt priority if none set
#define DEFAULT_RTPRIORITY 6


REGISTER_RDK_PLUGIN(RtSchedulingPlugin);

RtSchedulingPlugin::RtSchedulingPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                       const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                       const std::string &rootfsPath,
                                       const std::string &hookStdin)
    : mName("RtScheduling"),
      mUtils(utils),
      mConfig(containerConfig),
      mRootfsPath(rootfsPath),
      mHookStdin(hookStdin)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

unsigned RtSchedulingPlugin::hookHints() const
{
    return IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
           IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag;
}

// -----------------------------------------------------------------------------
/**
 *  @brief postInstallation OCI hook.
 *
 *  TODO:
 *
 *  @return true on success, false on failure.
 */
bool RtSchedulingPlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    // if no rtpriority value given, use default
    int rtPriorityLimit = mConfig->rdk_plugins->rtscheduling->data->rtlimit;
    if (rtPriorityLimit == 0)
    {
        rtPriorityLimit = DEFAULT_RTPRIORITY;
    }

    // force to range 1 - 99
    rtPriorityLimit = std::min<int>(rtPriorityLimit, 99);
    rtPriorityLimit = std::max<int>(rtPriorityLimit, 1);

    for (int i = 0; i < mConfig->process->rlimits_len; i++)
    {
        if (strcmp(mConfig->process->rlimits[i]->type, "RLIMIT_RTPRIO") == 0)
        {
            // found RLIMIT_RTPRIO, insert limit to it
            mConfig->process->rlimits[i]->hard = rtPriorityLimit;
            mConfig->process->rlimits[i]->hard_present = true;
            mConfig->process->rlimits[i]->soft = rtPriorityLimit;
            mConfig->process->rlimits[i]->soft_present = true;
            AI_LOG_FN_EXIT();
            return true;
        }
    }

    // RLIMIT_RTPRIO not found in rlimits so we have to add it ourselves

    // allocate memory to create new rlimit
    rt_dobby_schema_process_rlimits_element *newRlimit = (rt_dobby_schema_process_rlimits_element*)calloc(1, sizeof(rt_dobby_schema_process_rlimits_element));
    newRlimit->type = strdup("RLIMIT_RTPRIO");
    newRlimit->hard = rtPriorityLimit;
    newRlimit->hard_present = true;
    newRlimit->soft = rtPriorityLimit;
    newRlimit->soft_present = true;

    // allocate memory for new rlimit in config and place it in
    mConfig->process->rlimits_len++;
    mConfig->process->rlimits = (rt_dobby_schema_process_rlimits_element**)realloc(mConfig->process->rlimits, sizeof(rt_dobby_schema_process_rlimits_element*) * mConfig->process->rlimits_len);
    mConfig->process->rlimits[mConfig->process->rlimits_len-1] = newRlimit;

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the RT scheduling priority on the container's init process
 *
 *  This simply calls sched_setscheduler on the supplied pid, inserting the
 *  given default rt priority value.
 *
 *  @return true if successiful otherwise false.
 */
bool RtSchedulingPlugin::createRuntime()
{
    AI_LOG_FN_ENTRY();

    // if no rtpriority value given, use default
    int rtPriorityLimit = mConfig->rdk_plugins->rtscheduling->data->rtlimit;
    if (rtPriorityLimit == 0)
    {
        rtPriorityLimit = DEFAULT_RTPRIORITY;
    }
    int rtPriorityDefault = mConfig->rdk_plugins->rtscheduling->data->rtdefault;
    if (rtPriorityDefault == 0)
    {
        rtPriorityDefault = DEFAULT_RTPRIORITY;
    }

    // force to range 1 - 99
    rtPriorityLimit = std::min<int>(rtPriorityLimit, 99);
    rtPriorityLimit = std::max<int>(rtPriorityLimit, 1);
    rtPriorityDefault = std::min<int>(rtPriorityDefault, 99);
    rtPriorityDefault = std::max<int>(rtPriorityDefault, 1);

    if (rtPriorityDefault > rtPriorityLimit)
    {
        AI_LOG_WARN("the default rt priority is higher than the limit");
    }

    // get the container pid
    pid_t containerPid = mUtils->getContainerPid(mHookStdin);
    if (!containerPid)
    {
        AI_LOG_ERROR_EXIT("couldn't find container pid");
        return false;
    }

    // set default rt limit
    struct sched_param schedParam;
    schedParam.sched_priority = rtPriorityDefault;
    if (sched_setscheduler(containerPid, SCHED_RR, &schedParam) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to set RR scheduling policy");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}
