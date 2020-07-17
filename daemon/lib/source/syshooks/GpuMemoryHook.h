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
 * File:   GpuMemoryHook.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef GPUMEMORYHOOK_H
#define GPUMEMORYHOOK_H

#include "BaseHook.h"

#include <memory>

class IDobbyEnv;

// -----------------------------------------------------------------------------
/**
 *  @class GpuMemoryHook
 *  @brief Sets the gpu memory limits for a given container.
 *
 *  This is a pre-start hook that simply creates a gpu cgroup for the container,
 *  sets the limit and then moves the containered process into it.
 *
 *  This is effectively what runc does for all the other limits, but obviously
 *  doesn't know about the gpu cgroup as that is a sky extension.
 *
 */
class GpuMemoryHook : public BaseHook
{
public:
    GpuMemoryHook(const std::shared_ptr<IDobbyEnv>& env,
                  const std::shared_ptr<IDobbyUtils>& utils);
    ~GpuMemoryHook() final;

public:
    std::string hookName() const override;

    unsigned hookHints() const override;

    bool preStart(const ContainerId& id,
                  pid_t containerPid,
                  const std::shared_ptr<const DobbyConfig>& config,
                  const std::shared_ptr<const DobbyRootfs>& rootfs) override;

    bool postStop(const ContainerId& id,
                  const std::shared_ptr<const DobbyConfig>& config,
                  const std::shared_ptr<const DobbyRootfs>& rootfs) override;

private:
    bool setupContainerGpuLimit(const ContainerId& id,
                                pid_t containerPid,
                                const std::shared_ptr<const DobbyConfig>& config);

    bool writeCgroupFile(const ContainerId& id, const std::string& fileName, size_t value);


    void bindMountGpuCgroup(const std::string& source,
                            const std::string& target);

    void unmountGpuCgroup(const std::string& mountPoint);

private:
    const std::shared_ptr<IDobbyUtils> mUtilities;

private:
    int mCgroupDirfd;
    const std::string mCgroupDirPath;
};


#endif // !defined(GPUMEMORYHOOK_H)
