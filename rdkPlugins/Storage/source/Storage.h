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
#include "BindLoopMountDetails.h"

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

    // This hook changes privlidges of the mounted directorires
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

    // In this hook there should be deletion of img file when non-
    // persistent option is selected
    bool postHalt() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    std::vector<MountProperties> getLoopMounts();
    std::vector<MountProperties> getBindLoopMounts();
    std::vector<std::unique_ptr<MountDetails>> getMountDetails();

    template<typename T>
    MountProperties CreateMountProperties(T *pMount)
    {
        MountProperties mount;
        mount.fsImagePath = std::string(pMount->source);
        mount.destination = std::string(pMount->destination);
        mount.mountFlags  = pMount->flags;

        // Optional parameters
        if (pMount->fstype)
        {
            mount.fsImageType = std::string(pMount->fstype);
        }
        else
        {
            // default image type = ext4
            mount.fsImageType = "ext4";
        }

        if (pMount->persistent_present)
        {
            mount.persistent = pMount->persistent;
        }
        else
        {
            // default persistent = true
            mount.persistent = true;
        }

        if (pMount->imgsize_present)
        {
            mount.imgSize = pMount->imgsize;
        }
        else
        {
            if (mount.fsImageType.compare("xfs") == 0)
            {
                // default image size = 16 MB
                mount.imgSize = 16 * 1024 * 1024;
            }
            else
            {
                // default image size = 12 MB
                mount.imgSize = 12 * 1024 * 1024;
            }
        }

        for (size_t j = 0; j < pMount->options_len; j++)
        {
            mount.mountOptions.push_back(std::string(pMount->options[j]));
        }

        if (pMount->imgmanagement_present)
        {
            mount.imgManagement = pMount->imgmanagement;
        }
        else
        {
            // default imgManagement = true
            mount.imgManagement = true;
        }

        return mount;
    }

    template<typename T>
    std::unique_ptr<T> CreateMountDetails(const MountProperties &properties)
    {
        uid_t tmp_uid = 0;
        gid_t tmp_gid = 0;

        // Firstly get uid/gid from process
        if(mContainerConfig->process &&
           mContainerConfig->process->user)
        {
            if (mContainerConfig->process->user->uid_present)
            {
                tmp_uid = mContainerConfig->process->user->uid;
            }

            if (mContainerConfig->process->user->gid_present)
            {
                tmp_gid = mContainerConfig->process->user->gid;
            }
        }

        // Then map it inside container
        tmp_uid = getMappedId(tmp_uid,
                                mContainerConfig->linux->uid_mappings,
                                mContainerConfig->linux->uid_mappings_len);

        tmp_gid = getMappedId(tmp_gid,
                                mContainerConfig->linux->gid_mappings,
                                mContainerConfig->linux->gid_mappings_len);


        // create the loop mount and make sure it was constructed
        auto loopMount = std::make_unique<T>(mRootfsPath,
                                             properties,
                                             tmp_uid,
                                             tmp_gid,
                                             mUtils);

        return loopMount;
    }

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
    uint32_t getMappedId(uint32_t id, rt_defs_id_mapping **mapping, size_t mapping_len);
};

#endif // !defined(STORAGE_H)
