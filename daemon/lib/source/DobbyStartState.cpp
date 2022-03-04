/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
 * File:   DobbyStartState.cpp
 *
 */
#include "DobbyStartState.h"
#include "DobbyConfig.h"

#include <Logging.h>

#include <algorithm>
#include <fcntl.h>
#include <unistd.h>


DobbyStartState::DobbyStartState(const std::shared_ptr<DobbyConfig>& config,
                                 const std::list<int>& files_)
    : mConfig(config)
    , mValid(false)
{
    AI_LOG_FN_ENTRY();

    // dup the supplied file descriptors to ensure that they don't disappear
    // from underneath us.  Any failure here is fatal and will result in an
    // invalid start state object
    std::list<int>::const_iterator it = files_.begin();
    for (; it != files_.end(); ++it)
    {
        int fd = fcntl(*it, F_DUPFD_CLOEXEC, 3);
        if (fd < 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "F_DUPFD_CLOEXEC failed");
            return;
        }

        mFiles.push_back({"Generic", fd});
    }

    // all fd's have been dup'ed so we're valid
    mValid = true;

    AI_LOG_FN_EXIT();
}

DobbyStartState::~DobbyStartState()
{
    AI_LOG_FN_ENTRY();

    // close all the file descriptors we've dup'd
    for (const auto& entry : mFiles)
    {
        int fd = entry.second;
        if ((fd >= 0) && (close(fd) != 0))
        {
            AI_LOG_SYS_ERROR(errno, "failed to close descriptor");
        }
    }

    AI_LOG_FN_EXIT();
}

bool DobbyStartState::isValid() const
{
    std::lock_guard<std::mutex> locker(mLock);
    return mValid;
}

// -------------------------------------------------------------------------
/**
 *  @brief Gets all file descriptor registered by any client
 *
 *  @return List of all file descriptors
 */
std::list<int> DobbyStartState::files() const
{
    std::lock_guard<std::mutex> locker(mLock);

    std::list<int> retVal;
    std::transform(mFiles.cbegin(), mFiles.cend(), std::back_inserter(retVal),
        [](const auto& entry)
        {
            return entry.second;
        });

    return retVal;
}

// -------------------------------------------------------------------------
/**
 *  @brief Gets all file descriptor registered by concrete client
 *
 *  @param[in]  pluginName  RDK plugin name
 *
 *  @return List of file descriptors assiociated with given plugin name
 */
std::list<int> DobbyStartState::files(const std::string& pluginName) const
{
    std::lock_guard<std::mutex> locker(mLock);

    std::list<int> retVal;
    for (const auto& entry : mFiles)
    {
        const auto& key = entry.first;
        const auto& value = entry.second;

        if (key == pluginName)
        {
            retVal.push_back(value);
        }
    }

    return retVal;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds another file descriptor to be passed into the container
 *
 *  The number of the file descriptor in the container namespace is returned,
 *  unless there was an error in which case a negative value is returned.
 *  File descriptors start at 3.
 *
 *  The method dups the supplied file descriptor so it can be closed immmediatly
 *  after the call.  The file descriptor will be closed after the container is
 *  started and handed over.
 *
 *  File descriptors are recorded per client (plugin name).
 *
 *  Lastly to help find issues, this function will log an error and reject the
 *  file descriptor if it doesn't have the FD_CLOEXEC bit set.
 *
 *  @param[in]  pluginName  The plugin name for which fd will be recorded
 *  @param[in]  fd      The file descriptor to pass to the container
 *
 *  @return the number of the file descriptor inside the container on success,
 *  on failure -1
 */
int DobbyStartState::addFileDescriptor(const std::string& pluginName, int fd)
{
    AI_LOG_FN_ENTRY();

    // first sanity check the FD_CLOEXEC flag is set, this is only here to
    // catch errors by hooks that don't create their fd with the correct flags
    int flags = fcntl(fd, F_GETFD, 0);
    if ((flags < 0) || !(flags & FD_CLOEXEC))
    {
        AI_LOG_ERROR_EXIT("fd is invalid or doesn't have the FD_CLOEXEC bit set");
        return -1;
    }

    // dup the fd
    int duppedFd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "F_DUPFD_CLOEXEC failed");
        return -1;
    }

    // take the lock and add the fd to the list
    std::lock_guard<std::mutex> locker(mLock);

    int containerFd = 3 + static_cast<int>(mFiles.size());
    mFiles.push_back({pluginName, duppedFd});

    AI_LOG_FN_EXIT();
    return containerFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds an environment variable to the container
 *
 *  Simple appends another environment variable to the container
 *
 *  @param[in]  envVar      The environment variable to set
 *
 *  @return true on success, false on failure
 */
bool DobbyStartState::addEnvironmentVariable(const std::string& envVar)
{
    // take the lock to protect the config object, currently the config object
    // is not thread safe so this lock is needed
    std::lock_guard<std::mutex> locker(mLock);
    return mConfig->addEnvironmentVar(envVar);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a new mount to the container
 *
 *  Adds a mount entry to the config.json for the container.
 *
 *  @warning this can't be used to add loopback mounts, only standard /dev
 *  mounts or bind mounts of directories and files.
 *
 *  @param[in]  source          The source of the mount
 *  @param[in]  target          The target mount point
 *  @param[in]  fsType          The filesystem type of the mount
 *  @param[in]  mountFlags      The mount flags (i.e. MS_BIND, MS_??)
 *  @param[in]  mountOptions    Any additional mount options.
 *
 *  @return true on success, false on failure
 */
bool DobbyStartState::addMount(const std::string& source,
                               const std::string& target,
                               const std::string& fsType,
                               unsigned long mountFlags /*= 0*/,
                               const std::list<std::string>& mountOptions /*= std::list<std::string>()*/)
{
    AI_LOG_INFO("adding mount ('%s', '%s', '%s', ...",
                source.c_str(), target.c_str(), fsType.c_str());

    // take the lock to protect the config object, currently the config object
    // is not thread safe so this lock is needed
    std::lock_guard<std::mutex> locker(mLock);
    return mConfig->addMount(source, target, fsType, mountFlags, mountOptions);
}

