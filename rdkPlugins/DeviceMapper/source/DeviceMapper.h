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

#ifndef DEVICEMAPPER_H
#define DEVICEMAPPER_H

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

/**
 * @brief Simple Dobby RDK Plugin
 *
 * Implements all hook points to print a simple statement showing the hook has been
 * called successfully.
 *
 * Can be used as a reference implementation for future plugins
 */
class DeviceMapperPlugin : public RdkPluginBase
{
public:
    DeviceMapperPlugin(std::shared_ptr<rt_dobby_schema>& containerConfig,
                  const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                  const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    // Override to return the appropriate hints for what we implement
    unsigned hookHints() const override;

public:

    bool preCreation() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    struct DevNode
    {
        std::string path;
        int64_t major;
        int64_t minor;
        int64_t configMajor;
        int64_t configMinor;
        mode_t mode;
    };

    bool getDevNodeFromPath(const std::string& path, DevNode& node);

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    bool mValid;
};

#endif // !defined(DEVICEMAPPER_H)

