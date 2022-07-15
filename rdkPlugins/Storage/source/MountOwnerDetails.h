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
 * File: MountOwnerDetails.h
 *
 */
#ifndef MOUNTOWNERDETAILS_H
#define MOUNTOWNERDETAILS_H

#include "MountProperties.h"

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <string>
#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @class MountOwnerDetails
 *  @brief This class is only intended to be used internally by Storage plugin
 *  do not use from external code.
 *
 *  @see Storage
 */
class MountOwnerDetails
{
public:
    MountOwnerDetails() = delete;
    MountOwnerDetails(MountOwnerDetails&) = delete;
    MountOwnerDetails(MountOwnerDetails&&) = delete;
    ~MountOwnerDetails();

private:
    friend class Storage;

public:
    MountOwnerDetails(const std::string& rootfsPath,
                      const MountOwnerProperties& mountOwnerProperties,
                      const uid_t& defaultUserId,
                      const gid_t& defaultGroupId,
                      const std::shared_ptr<DobbyRdkPluginUtils> &utils);

public:
    bool onCreateRuntime() const;

private:
    bool getOwnerIds(uid_t& userId, gid_t& groupId) const;
    bool processOwners() const;
    bool changeOwnerRecursive(const std::string& path, uid_t userId, gid_t groupId) const;
    bool changeOwner(const std::string& path, uid_t userId, gid_t groupId) const;

    const std::string mRootfsPath;
    MountOwnerProperties mMountOwnerProperties;
    uid_t mDefaultUserId;
    gid_t mDefaultGroupId;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(MOUNTOWNERDETAILS_H)
