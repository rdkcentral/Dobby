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

#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>

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
 *  @brief Creates destination path so it exists before mounting
 *
 *  @return true on success, false on failure.
 */
bool DynamicMountDetails::onCreateRuntime() const
{
    AI_LOG_FN_ENTRY();

    bool success = false;
    std::string targetPath = mRootfsPath + mMountProperties.destination;
    std::string dirPath;

    struct stat buffer;
    if (stat(mMountProperties.source.c_str(), &buffer) == 0)
    {
        bool isDir = S_ISDIR(buffer.st_mode);
        // Determine path based on whether source is a directory or file
        if (isDir)
        {
            dirPath = targetPath;
        }
        else
        {
            // Mounting a file so exclude filename from directory path
            std::size_t found = targetPath.find_last_of("/");
            dirPath = targetPath.substr(0, found);
        }

        // Recursively create destination directory structure
        if (mUtils->mkdirRecursive(dirPath, 0755) || (errno == EEXIST))
        {
            if (isDir)
            {
                success = true;
            }
            else
            {
                // If mounting a file, make sure a file with the same name
                // exists at the desination path prior to bind mounting.
                // Otherwise the bind mount may fail if the destination path
                // filesystem is read-only.
                // Creating the file first ensures an inode exists for the
                // bind mount to target.
                int fd = open(targetPath.c_str(), O_RDONLY | O_CREAT, 0644);
                if (fd >= 0)
                {
                    close(fd);
                    success = true;
                }
                else
                {
                    AI_LOG_SYS_ERROR(errno, "failed to open or create destination '%s'", targetPath.c_str());
                }
            }
        }
        else
        {
            AI_LOG_SYS_ERROR(errno, "failed to create mount destination path '%s' in storage plugin", targetPath.c_str());
        }
    }
    else
    {
        // No mount source so ignore
        success = true;
        AI_LOG_INFO("Source '%s' does not exist, dynamic mount directory creation skipped", mMountProperties.source.c_str());
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds bind mount only if source exists on the host
 *
 *  @return true on success, false on failure.
 */
bool DynamicMountDetails::onCreateContainer() const
{
    AI_LOG_FN_ENTRY();

    bool success = false;
    std::string targetPath = mRootfsPath + mMountProperties.destination;

    struct stat buffer;
    if (stat(mMountProperties.source.c_str(), &buffer) == 0)
    {
        bool isDir = S_ISDIR(buffer.st_mode);
        if (stat(targetPath.c_str(), &buffer) != 0)
        {
            std::string dirPath; 
            // Determine path based on whether target is a directory or file
            if (isDir)
            {
                dirPath = targetPath;
            }
            else
            {
                // Mounting a file so exclude filename from directory path
                std::size_t found = targetPath.find_last_of("/");
                dirPath = targetPath.substr(0, found);
            }

            // Recursively create destination directory structure
            if (mUtils->mkdirRecursive(dirPath, 0755) || (errno == EEXIST))
            {
                if (isDir)
                {
                    success = true;
                }
                else
                {
                    // If mounting a file, make sure a file with the same name
                    // exists at the desination path prior to bind mounting.
                    // Otherwise the bind mount may fail if the destination path
                    // filesystem is read-only.
                    // Creating the file first ensures an inode exists for the
                    // bind mount to target.
                    int fd = open(targetPath.c_str(), O_RDONLY | O_CREAT, 0644);
                    if ((fd == 0) || (errno == EEXIST))
                    {
                        close(fd);
                        success = true;
                    }
                    else
                    {
                        AI_LOG_SYS_ERROR(errno, "failed to open or create destination '%s'", targetPath.c_str());
                    }
                }
            }
            else
            {
                AI_LOG_SYS_ERROR(errno, "failed to create mount destination path '%s' in storage plugin", targetPath.c_str());
            }
        }
        success = addMount();
    }
    else
    {
        // No mount source so ignore
        success = true;
        AI_LOG_INFO("Source '%s' does not exist, dynamic mount skipped", mMountProperties.source.c_str());
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Unmounts dynamic mounts
 *
 *  @return true on success, false on failure.
 */
bool DynamicMountDetails::onPostStop() const
{
    AI_LOG_FN_ENTRY();

    bool success = false;
    std::string targetPath = mRootfsPath + mMountProperties.destination;
    struct stat buffer;

    if (stat(targetPath.c_str(), &buffer) == 0)
    {
        if (remove(targetPath.c_str()) == 0)
        {
            success = true;
        }
        else
        {
            AI_LOG_SYS_ERROR(errno, "failed to remove dynamic mount '%s' in storage plugin", targetPath.c_str());
        }
    }
    else
    {
        success = true;
        AI_LOG_INFO("Mount '%s' does not exist, dynamic mount skipped", targetPath.c_str());
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
bool DynamicMountDetails::addMount() const
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

    // Bind mount source into destination
    std::string targetPath = mRootfsPath + mMountProperties.destination;
    if (mount(mMountProperties.source.c_str(),
              targetPath.c_str(),
              "",
              mMountProperties.mountFlags | MS_BIND,
              mountData.data()) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to add dynamic mount '%s' in storage plugin", targetPath.c_str());
        return false;
    }

    return true;
}
