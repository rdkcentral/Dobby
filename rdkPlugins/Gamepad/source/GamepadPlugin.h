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
 * @brief Dobby Gamepad plugin.
 *
 */
class GamepadPlugin : public RdkPluginBase
{
public:
    GamepadPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
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

public:
    std::vector<std::string> getDependencies() const override;

private:
    void addDevices(int64_t major, int64_t minor, int numDevices, const std::string& type, const std::string& mode) const;
    void addGidMapping(gid_t host_id, gid_t container_id) const;
    void addAdditionalGid(gid_t gid) const;
    gid_t getInputGroupId() const;

    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(GPUPLUGIN_H)
