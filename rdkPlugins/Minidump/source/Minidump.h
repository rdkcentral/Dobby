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
 * File: Minidump.h
 *
 */
#ifndef MINIDUMP_H
#define MINIDUMP_H

#include <RdkPluginBase.h>

#include <string>
#include <memory>

#include "LoopMountDetails.h"
#include "MappedId.h"

/**
 * @brief Dobby RDK Minidump Plugin
 *
 * Manages collection of minidump core files located in container namespace
 */
class Minidump : public RdkPluginBase
{
public:
    Minidump(std::shared_ptr<rt_dobby_schema>& containerConfig,
             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
             const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    // This hook attaches img file to loop device and mount it inside
    // temp mount point (within container rootfs)
    bool preCreation() override;

    // This hook changes privileges of the mounted directorires
    bool createRuntime() override;

    // This hook mounts temp directory to the proper one
    bool createContainer() override;

    // Cleaning up temp mount
    bool postStart() override;

    // In this hook there should be deletion of img file
    bool postStop() override;

    // This hook copies minidump file to host namespace
    bool postHalt() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    struct PathData
    {
        PathData(const std::string& image,
                 const std::string& containerSource,
                 const std::string& hostDestination,
                 const int imgSize)
            : image(image)
            , containerSource(containerSource)
            , hostDestination(hostDestination)
            , imgSize(imgSize)
        {
        }

        const std::string image;
        const std::string containerSource;
        const std::string hostDestination;
        const int imgSize;
    };

private:
    std::vector<PathData> getPathsData();
    std::unique_ptr<LoopMountDetails> convert(const PathData& pathData);

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const MappedId mMappedId;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(MINIDUMP_H)
