/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * File:   DobbyBundle.cpp
 *
 * Copyright (C) BSKYB 2016+
 */
#include "DobbyBundle.h"
#include "IDobbyUtils.h"
#include "IDobbyEnv.h"

#include <Logging.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string>
#include <sstream>
#include <random>

// -----------------------------------------------------------------------------
/**
 *  @brief Constructor only intended for debugging.
 *
 *  Creates a bundle directory at the given path and doesn't delete it at
 *  when destructed.  This is used for the debug dbus interface:
 *
 *      "com.sky.dobby.debug1.CreateBundle"
 *
 *  Which is helpful for debugging container start-up issues.
 *
 *  @param[in]  utils       The daemon utils object
 *  @param[in]  path        The absolute path to a directory to create
 *  @param[in]  persist     If true the directory is not deleted when destroyed
 *
 */
DobbyBundle::DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,
                         const std::string& path_,
                         bool persist /*= true*/)
    : mUtilities(utils)
    , mPersist(persist)
    , mDirFd(-1)
{
    AI_LOG_FN_ENTRY();

    if (mkdir(path_.c_str(), 0755) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create bundle dir @ '%s'",
                         path_.c_str());
    }
    else
    {
        mDirFd = open(path_.c_str(), O_CLOEXEC | O_DIRECTORY);
        if (mDirFd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to open bundle dir @ '%s'",
                             path_.c_str());
        }
        else if (fchmod(mDirFd, 0755) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to set 0755 mode on dir @ '%s'",
                             path_.c_str());

            close(mDirFd);
            mDirFd = -1;
        }
        else
        {
            // all good in the hood, so store the complete path to the newly
            // created bundle dir
            mPath = path_;
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructor that creates a directory within the bundles dir with a
 *  name that matches the id
 *
 *  The created directory and it's contents are deleted in the destructor.
 *
 *  Bundles are created in a sub-dir of the workspace, i.e.
 *  <workspace>/dobby/bundles/<id>/
 *
 *
 *  @param[in]  utils       The daemon utils object.
 *  @param[in]  env         The daemon's environment object, used to get the
 *                          location of the bundles dir.
 *  @param[in]  id          The id of the container, used as the name for the
 *                          bundle dir.
 */
DobbyBundle::DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,
                         const std::shared_ptr<const IDobbyEnv>& env,
                         const ContainerId& id)
    : mUtilities(utils)
    , mPersist(false)
    , mDirFd(-1)
{
    AI_LOG_FN_ENTRY();

    // the subdir should already be created, but in case it hasn't we allow
    // it to be created here
    const std::string bundlesPath(env->workspaceMountPath() + "/dobby/bundles/");
    int bundlesDirFd = open(bundlesPath.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (bundlesDirFd < 0)
    {
        if (errno == ENOENT)
        {
            // the directory doesn't exist so create it
            if (!mUtilities->mkdirRecursive(bundlesPath, 0755))
            {
                AI_LOG_ERROR_EXIT("failed to create bundles dir @ '%s'",
                                  bundlesPath.c_str());
                return;
            }

            // try and open the dir now we've created it
            bundlesDirFd = open(bundlesPath.c_str(), O_CLOEXEC | O_DIRECTORY);
        }

        // check again we managed to open the directory
        if (bundlesDirFd < 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to open bundles dir @ '%s'",
                                  bundlesPath.c_str());
            return;
        }

        // enforce the access modes in case someone has messed with the umask
        if (fchmod(bundlesDirFd, 0755) != 0)
        {
            AI_LOG_SYS_WARN(errno, "failed to set 0755 mode on bundles dir");
        }
    }

    // create the directory as a subdirectory in the bundles dir, note that the
    // name of the directory is labeled with the container id, plus a random
    // number therefore it is possible to have two bundles with the same id.
    // The number was added because in a rare number of cases the unmount of the
    // private dir is succeeding but removing the private dir returns EBUSY.
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<int> dist(10000, 99999);

    std::ostringstream os;
    os << id.str() << "." << dist(mt);
    std::string dir_name = os.str();
    // eg. com.bskyb.epgui.12345

    if (mkdirat(bundlesDirFd, dir_name.c_str(), 0755) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create bundle dir @ '%s/%s'",
                         bundlesPath.c_str(), dir_name.c_str());
    }
    else
    {
        mDirFd = openat(bundlesDirFd, dir_name.c_str(), O_CLOEXEC | O_DIRECTORY);
        if (mDirFd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to open bundle dir @ '%s/%s'",
                             bundlesPath.c_str(), dir_name.c_str());
        }
        else if (fchmod(mDirFd, 0755) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to set 0755 mode on dir @ '%s/%s'",
                             bundlesPath.c_str(), dir_name.c_str());

            close(mDirFd);
            mDirFd = -1;
        }
        else
        {
            // all good in the hood, so store the complete path to the newly
            // created bundle dir
            mPath = bundlesPath + dir_name;
        }
    }

    if (close(bundlesDirFd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close bundle dir");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructor that populates member variables in the object based on
 *  bundlePath.
 *
 *  @param[in]  utils       The daemon utils object.
 *  @param[in]  env         The daemon's environment object, used to get the
 *                          location of the bundles dir.
 *  @param[in]  bundlePath  Path to an OCI bundle.
 */
DobbyBundle::DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,
                         const std::shared_ptr<const IDobbyEnv>& env,
                         const std::string& bundlePath)
    : mUtilities(utils)
    , mPersist(true)
    , mDirFd(-1)
{
    AI_LOG_FN_ENTRY();

    // no sanity check needed, bundlePath has already been opened and processed in DobbyConfig
    mPath = bundlePath;

    // get the directory's file descriptor
    mDirFd = open(bundlePath.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (mDirFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open bundle dir @ '%s'", bundlePath.c_str());
    }
    else if (fchmod(mDirFd, 0755) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "bundle dir @ '%s' does not have 0755 mode", bundlePath.c_str());

        close(mDirFd);
        mDirFd = -1;
    }

    AI_LOG_FN_EXIT();
}

DobbyBundle::~DobbyBundle()
{
    AI_LOG_FN_ENTRY();

    if (mDirFd >= 0)
    {
        if (!mPersist && !mUtilities->rmdirContents(mDirFd))
        {
            AI_LOG_ERROR("failed to delete contents of bundle dir");
        }
        if (close(mDirFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to close bundle dir");
        }
    }

    // the bundle directory should now be empty, so can now delete it
    if (!mPath.empty())
    {
        if (!mPersist && (rmdir(mPath.c_str()) != 0))
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete bundle dir");
        }
    }

    AI_LOG_FN_EXIT();
}

void DobbyBundle::setPersistence(bool persist)
{
    mPersist = persist;
}

const bool DobbyBundle::getPersistence() const
{
    return mPersist;
}

bool DobbyBundle::isValid() const
{
    return (mDirFd >= 0) && !mPath.empty();
}

const std::string& DobbyBundle::path() const
{
    return mPath;
}

int DobbyBundle::dirFd() const
{
    return mDirFd;
}

