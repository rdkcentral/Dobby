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
/*
 * File: Storage.h
 *
 */
#ifndef STORAGE_H
#define STORAGE_H

#include "LoopMountDetails.h"
#include "DynamicMountDetails.h"
#include "MountOwnerDetails.h"

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string>

//#define ENABLE_TESTS    1

/**
 * @brief Dobby RDK Storage Plugin
 *
 * Manages loop mount devices for containers
 */
class Storage : public RdkPluginBase
{
public:
    Storage(std::shared_ptr<rt_dobby_schema>& containerConfig,
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
    // This hook attaches img file to loop device and mount it inside
    // temp mount point (within container rootfs)
    bool preCreation() override;

    // This hook changes privileges of the mounted directories
    bool createRuntime() override;

    // This hook mounts temp directory to the proper one
    bool createContainer() override;

#ifdef ENABLE_TESTS
    // Used only for testing purpose
    bool startContainer() override;
#endif // ENABLE_TESTS

    // Cleaning up temp mount
    bool postStart() override;

    // In this hook there should be deletion of img file when non-
    // persistent option is selected
    bool postStop() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    std::vector<LoopMountProperties> getLoopMounts() const;
    std::vector<std::unique_ptr<LoopMountDetails>> getLoopMountDetails() const;

    std::vector<DynamicMountProperties> getDynamicMounts() const;
    std::vector<std::unique_ptr<DynamicMountDetails>> getDynamicMountDetails() const;

    std::vector<MountOwnerProperties> getMountOwners() const;
    std::vector<std::unique_ptr<MountOwnerDetails>> getMountOwnerDetails() const;

    void setupOwnerIds(uid_t& uid, gid_t& gid) const;

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;

    uint32_t getMappedId(uint32_t id, rt_defs_id_mapping **mapping, size_t mapping_len) const;
};

#endif // !defined(STORAGE_H)
