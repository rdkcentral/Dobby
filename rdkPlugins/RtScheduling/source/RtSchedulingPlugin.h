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

#ifndef RTSCHEDULINGPLUGIN_H
#define RTSCHEDULINGPLUGIN_H

#include <RdkPluginBase.h>

/**
 * @brief Dobby RtScheduling plugin.
 *
 *  This plugin sets the RT priority of the container.
 *
 *  This consists of a postInstallation hook that adds the rtpriority
 *  limit and createRuntime hook that calls sched_setscheduler on the
 *  container's init process.
 *
 *  Due to the way scheduling is inherited this will in turn filter down to
 *  all processes running within the container.
 *
 */
class RtSchedulingPlugin : public RdkPluginBase
{
public:
    RtSchedulingPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                    const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                    const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    bool postInstallation() override;
    bool createRuntime() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    const std::string mName;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    std::shared_ptr<rt_dobby_schema> mConfig;
    const std::string mRootfsPath;
};

#endif // !defined(RTSCHEDULINGPLUGIN_H)
