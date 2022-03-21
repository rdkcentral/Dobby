/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2022 Sky UK
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
 * File: BindLoopMountDetails.h
 *
 */
#ifndef BINDLOOPMOUNTDETAILS_H
#define BindLOOPMOUNTDETAILS_H

#include "MountDetails.h"
#include "MountCommon.h"

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <string>
#include <list>
#include <memory>

class RefCountFile;


// -----------------------------------------------------------------------------
/**
 *  @class BindLoopMountDetails
 *  @brief Class that represents a single loop mount within a container
 *
 *  This class is only intended to be used internally by Storage plugin
 *  do not use from external code.
 *
 *  @see Storage
 */
class BindLoopMountDetails : public MountDetails
{
public:
    BindLoopMountDetails() = delete;
    BindLoopMountDetails(BindLoopMountDetails&) = delete;
    BindLoopMountDetails(BindLoopMountDetails&&) = delete;
    ~BindLoopMountDetails();

private:
    friend class Storage;

public:
    BindLoopMountDetails(const std::string& rootfsPath,
                     const MountProperties& mount,
                     const uid_t& userId,
                     const gid_t& groupId,
                     const std::shared_ptr<DobbyRdkPluginUtils> &utils);

public:
    bool doLoopMount(const std::string& loopDevice) override;

    bool onPreCreate() override;

    bool setPermissions() override;

    bool remountTempDirectory() override;

    bool cleanupTempDirectory() override;

    void decrementReferenceCount() override;

    bool removeNonPersistentImage() override;

private:
    std::unique_ptr<RefCountFile> getRefCountFile();
    void unmountLoopbackDevice();

private:
    std::string mMountPointInsideContainer;
    std::string mTempMountPointOutsideContainer;
    MountProperties mMount;
    uid_t mUserId;
    gid_t mGroupId;

    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(BINDLOOPMOUNTDETAILS_H)
