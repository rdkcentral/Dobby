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

#include "DynamicMountDetails.h"
#include "StorageHelper.h"
#include "DobbyRdkPluginUtils.h"

#include <fstream>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sstream>
#include <fstream>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>

constexpr char MOUNT_TYPE[] = "bind";

// Dynamic mount details constructor
DynamicMountDetails::DynamicMountDetails(const std::string& rootfsPath,
                                         const DynamicMountProperties& mountProperties,
                                         const std::shared_ptr<DobbyRdkPluginUtils> &utils)
    : mRootfsPath(rootfsPath),
      mMountProperties(mountProperties),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

DynamicMountDetails::~DynamicMountDetails()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks whether source exists and creates bind mount if found
 *
 *  @return true on success, false on failure.
 */
bool DynamicMountDetails::onCreateContainer()
{
    AI_LOG_FN_ENTRY();
    bool success = false;

    struct stat buffer;
    if (stat(mMountProperties.source.c_str(), &buffer) == 0)
    {
        success = addMount();
    }
    else
    {
        // No mount source so ignore
        success = true;
    }
    
    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Add mount between source and destination.
 *
 *  @return true on success, false on failure.
 */
bool DynamicMountDetails::addMount()
{   
    // Create comma separated string of mount options 
    std::string mountData;
    std::list<std::string>::const_iterator it = mMountProperties.mountOptions.begin();
    for (; it != mMountProperties.mountOptions.end(); ++it)
    {
        if (it != mMountProperties.mountOptions.begin())
            mountData += ",";
         mountData += *it;    
    }
    
    std::string targetPath = mRootfsPath + mMountProperties.destination;    
    
    // Create target file on host within the container rootfs
    std::ofstream targetFile(targetPath);
    targetFile.close();

    // Bind mount source into destination
    if (mount(mMountProperties.source.c_str(),
              targetPath.c_str(),
              "", 
              mMountProperties.mountFlags | MS_BIND, 
              mountData.data()) != 0)
    {
        AI_LOG_ERROR("failed to add dynamic mount '%s' in storage plugin",
                     mMountProperties.source.c_str());
        return false;
    }

    return true;
}
