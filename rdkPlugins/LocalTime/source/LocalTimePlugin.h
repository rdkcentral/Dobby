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

#ifndef LOCALTIMEPLUGIN_H
#define LOCALTIMEPLUGIN_H

#include <RdkPluginBase.h>

/**
 * @brief Dobby LocalTime plugin.
 *
 * This plugin simply creates a symlink to the real /etc/localtime file
 * in the rootfs of the container.
 *
 */
class LocalTimePlugin : public RdkPluginBase
{
public:
    LocalTimePlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
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

private:
    const std::string mName;
    const std::string mRootfsPath;
};

#endif // !defined(LOCALTIMEPLUGIN_H)
