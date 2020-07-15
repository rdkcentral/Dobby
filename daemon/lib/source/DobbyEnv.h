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
 * File:   DobbyEnv.ch
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef DOBBYENV_H
#define DOBBYENV_H

#include <IDobbyEnv.h>
#include <IDobbySettings.h>

#include <cstdint>
#include <string>
#include <memory>
#include <map>

// -----------------------------------------------------------------------------
/**
 *  @class DobbyEnv
 *  @brief Basic class used to store the stb environment.
 *
 *  Used to store constant stuff, like the flash mount point and platform type.
 *
 *  An instance of this class is passed to all plugins when they are initialised
 *  as well as storing information for the daemon's own use.
 *
 */
class DobbyEnv : public IDobbyEnv
{
public:
    explicit DobbyEnv(const std::shared_ptr<const IDobbySettings>& settings);
    ~DobbyEnv() final = default;

public:
    std::string workspaceMountPath() const override;

    std::string flashMountPath() const override;

    std::string pluginsWorkspacePath() const override;

    std::string cgroupMountPath(Cgroup cgroup) const override;

    uint16_t platformIdent() const override;

private:
    static std::map<IDobbyEnv::Cgroup, std::string> getCgroupMountPoints();
    static uint16_t getPlatformIdent();

private:
    const std::string mWorkspacePath;
    const std::string mFlashMountPath;
    const std::string mPluginsWorkspacePath;
    const std::map<IDobbyEnv::Cgroup, std::string> mCgroupMountPaths;
    const uint16_t mPlatformIdent;
};


#endif // !defined(DOBBYENV_H)
