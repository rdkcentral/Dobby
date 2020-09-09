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

#ifndef GPUPLUGIN_H
#define GPUPLUGIN_H

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

/**
 * @brief Dobby GPU plugin.
 *
 *  Sets the gpu memory limits for a given container.
 *
 *  This plugin simply creates a gpu cgroup for the container, sets the limit
 *  and then moves the containered process into it.
 *
 *  This is effectively what crun does for all the other limits, but it
 *  doesn't know about the custom gpu cgroup as that is an extension to the
 *  default cgroups.
 */
class GpuPlugin : public RdkPluginBase
{
public:
    GpuPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
              const std::shared_ptr<DobbyRdkPluginUtils> &utils,
              const std::string &rootfsPath,
              const std::string &hookStdin);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    bool createRuntime() override;
    bool postStop() override;

private:
    std::string getGpuCgroupMountPoint();

    bool setupContainerGpuLimit(const std::string cgroupDirPath,
                                pid_t containerPid,
                                int memoryLimit);

    bool bindMountGpuCgroup(const std::string &source,
                            const std::string &target);

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    const std::string mContainerId;
    const std::string mHookStdin;
};

#endif // !defined(GPUPLUGIN_H)
