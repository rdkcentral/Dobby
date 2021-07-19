/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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
 * File: IonMemoryPlugin.h
 *
 */
#ifndef IONMEMORYPLUGIN_H
#define IONMEMORYPLUGIN_H

#include <RdkPluginBase.h>
#include <map>

/**
 *  @class IonMemoryPlugin
 *  @brief Plugin used to setup the ION cgroup controller for the container.
 *
 *  ION is the raw memory allocator from Android, it is used on RDK platforms
 *  by some vendors to allocate memory buffers for the following systems:
 *    - (wayland) EGL / OpenGL surface buffers
 *    - gstreamer / OMX Media decode buffers
 *
 */
class IonMemoryPlugin final : public RdkPluginBase
{
public:
    IonMemoryPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                    const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                    const std::string &rootfsPath);

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
    std::string findIonCGroupMountPoint() const;
    bool setupContainerIonLimits(const std::string &cGroupDirPath,
                                 pid_t containerPid,
                                 const std::map<std::string, uint64_t> &heapLimits,
                                 uint64_t defaultLimit);

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    const std::string mRootfsPath;

    bool mValid;
    const rt_defs_plugins_ion_memory_data *mPluginData;
};

#endif // !defined(IONMEMORYPLUGIN_H)
