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
 * File: DynamicMountDetails.h
 *
 */
#ifndef DYNAMICMOUNTDETAILS_H
#define DYNAMICMOUNTDETAILS_H

#include "MountProperties.h"

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <string>
#include <list>
#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @class DynamicMountDetails
 *  @brief Class that represents a single mount within a container when the
 * source exists on the host.
 *
 *  This class is only intended to be used internally by Storage plugin
 *  do not use from external code.
 *
 *  @see Storage
 */
class DynamicMountDetails
{
public:
    DynamicMountDetails() = delete;
    DynamicMountDetails(DynamicMountDetails&) = delete;
    DynamicMountDetails(DynamicMountDetails&&) = delete;
    ~DynamicMountDetails();

private:
    friend class Storage;

public:
    DynamicMountDetails(const std::string& rootfsPath,
                        const DynamicMountProperties& mount,
                        const std::shared_ptr<DobbyRdkPluginUtils> &utils);

public:
    bool onCreateRuntime() const;
    bool onCreateContainer() const;

private:
    bool addMount() const;

    const std::string mRootfsPath;
    DynamicMountProperties mMountProperties;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(DYNAMICMOUNTDETAILS_H)
