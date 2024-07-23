/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or imied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/

#include "AndroidRuntimePlugin.h"
#include "AndroidHelper.h"
#include "rt_defs_plugins.h"

#include <Logging.h>

#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

REGISTER_RDK_PLUGIN(AndroidRuntimePlugin);

AndroidRuntimePlugin::AndroidRuntimePlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                 const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 const std::string &rootfsPath)
    : mName("AndroidRuntime"),
      mRootfsPath(rootfsPath),
      mContainerConfig(containerConfig),
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();

    if (mContainerConfig == nullptr ||
        mContainerConfig->rdk_plugins->androidruntime == nullptr ||
        mContainerConfig->rdk_plugins->androidruntime->data == nullptr)
    {
        AI_LOG_ERROR("No AndroidRuntime configuration provided");
        return;
    }

    const rt_defs_plugins_android_runtime_data *pluginData = mContainerConfig->rdk_plugins->androidruntime->data;

    if(pluginData->system_path == nullptr)
    {
        AI_LOG_ERROR("No path to system.img provided");
        return;
    }
    mSystemPath = std::string(pluginData->system_path);
    AI_LOG_INFO("Android system.img path is %s\n", mSystemPath.c_str());

    if(pluginData->vendor_path == nullptr)
    {
        AI_LOG_ERROR("No path to vendor.img provided");
        return;
    }
    mVendorPath = std::string(pluginData->vendor_path);
    AI_LOG_INFO("Android vendor.img path is %s\n", mVendorPath.c_str());

    if(pluginData->data_path == nullptr)
    {
        AI_LOG_ERROR("No path to data directory provided");
        return;
    }
    mDataPath = std::string(pluginData->data_path);
    AI_LOG_INFO("Android userdata path is %s\n", mDataPath.c_str());

    if(pluginData->cache_path == nullptr)
    {
        AI_LOG_ERROR("No path to cache directory provided");
        return;
    }
    mCachePath = std::string(pluginData->cache_path);
    AI_LOG_INFO("Android cache path is %s\n", mCachePath.c_str());

    if(pluginData->cmdline_path == nullptr)
    {
        AI_LOG_ERROR("No path to kernel commandline file provided");
        return;
    }
    mCmdlinePath = std::string(pluginData->cmdline_path);
    AI_LOG_INFO("Android cmdline path is %s\n", mCmdlinePath.c_str());

    mValid = true;
    AI_LOG_INFO("Started Android runtime plugin");
    AI_LOG_FN_EXIT();
}

unsigned AndroidRuntimePlugin::hookHints() const
{
    return IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
           IDobbyRdkPlugin::HintFlags::PostHaltFlag;
}

// -----------------------------------------------------------------------------
/**
 *  @brief postInstalllation OCI hook.
 *
 *  If set_tz parameter is set then its value should be a path to file.
 *  Read this file and put its contents into containers TZ env var.
 *
 *  @return true on success, false on failure.
 */
bool AndroidRuntimePlugin::postInstallation()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR("Configuration not valid - not mounting");
        return false;
    }

    doMounts();

    std::string etcPath = mRootfsPath + "etc";
    struct stat s;

    // There may be a symlink from /etc to /system/etc in the container, remove it if present
    // so that the Networking plugin can populate /etc
    if (lstat(etcPath.c_str(), &s) == 0)
    {
        if (S_ISLNK(s.st_mode))
        {
            if(unlink(etcPath.c_str()) < 0)
            {
                AI_LOG_SYS_ERROR_EXIT(errno, "Failed to remove %s symlink", etcPath.c_str());
                return false;
            }
            else
            {
                AI_LOG_INFO("Removed symlink %s", etcPath.c_str());
            }
        }
    }
    else
    {
        AI_LOG_INFO("%s cannot be statted", etcPath.c_str());
    }

    if (mUtils->mkdirRecursive(mRootfsPath + "/etc", 0755))
    {
        AI_LOG_INFO("Ensured /etc exists in rootfs");
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief postHalt OCI hook.
 *
 *
 *  @return true on success, false on failure.
 */
bool AndroidRuntimePlugin::postHalt()
{
    AI_LOG_FN_ENTRY();

    doUnmounts();

    AI_LOG_FN_EXIT();
    return true;
}


/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> AndroidRuntimePlugin::getDependencies() const
{
    std::vector<std::string> dependencies;

    return dependencies;
}

bool AndroidRuntimePlugin::doLoopMount(const std::string &src, const std::string &dest)
{
    AI_LOG_FN_ENTRY();
    int fdSrc = open(src.c_str(), O_RDWR);

    if (fdSrc < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open source file %s", src.c_str());
        return false;
    }

    if (!mUtils->mkdirRecursive(dest, 0755))
    {
        AI_LOG_ERROR_EXIT("Failed to create loop mount destination directory %s", dest.c_str());
        return false;
    }

    std::string deviceLoop;
    int fdLoop = AndroidHelper::openLoopDevice(&deviceLoop);
    if (fdLoop < 0)
    {
        close(fdSrc);
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open free loop device");
        return false;
    }

    int fdAttached = AndroidHelper::attachFileToLoopDevice(fdLoop, fdSrc);
    if (fdAttached < 0)
    {
        close(fdSrc);
        close(fdLoop);
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open free loop device");
        return false;
    }

    if (mount(deviceLoop.c_str(), dest.c_str(), "ext4", 0, NULL) < 0)
    {
        close(fdAttached);
        close(fdSrc);
        close(fdLoop);
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to mount system.img");
        return false;
    }

    close(fdLoop); // fd is associated to mount now, so close this one so loop device will auto-free when unmounted

    AI_LOG_FN_EXIT();
    return true;
}

bool AndroidRuntimePlugin::doBindMount(const std::string &src, const std::string &dest)
{
    AI_LOG_FN_ENTRY();
    if (access(src.c_str(), F_OK) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open source file %s", src.c_str());
        return false;
    }

    if (!mUtils->mkdirRecursive(dest, 0755))
    {
        AI_LOG_ERROR_EXIT("Failed to create bind mount destination directory %s", dest.c_str());
        return false;
    }

    if (mount(src.c_str(), dest.c_str(), NULL, MS_BIND, nullptr) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Mount failed %s->%s", src.c_str(), dest.c_str());
        return false;
    }

    return true;
}

bool AndroidRuntimePlugin::doBindFile(const std::string &src, const std::string &dest)
{
    AI_LOG_FN_ENTRY();
    if (access(src.c_str(), F_OK) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open source file %s", src.c_str());
        return false;
    }

    int fdDest = open(dest.c_str(), O_RDWR | O_CREAT);
    if (fdDest < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to open destination file for bind %s", dest.c_str());
        return false;
    }
    close(fdDest);

    if (mount(src.c_str(), dest.c_str(), NULL, MS_BIND, nullptr) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Bind mount file failed %s->%s", src.c_str(), dest.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

#define MOUNT_VENDOR  "/vendor"
#define MOUNT_DATA    "/data"
#define MOUNT_CACHE   "/cache"
#define MOUNT_CMDLINE "/cmdline"

bool AndroidRuntimePlugin::doMounts()
{
    AI_LOG_FN_ENTRY();

    if(!mMounted.empty())
    {
        AI_LOG_ERROR("Attempt to mount again before unmounting previous");
        return false;
    }

    std::string dest;

    dest = mRootfsPath;
    if (!doLoopMount(mSystemPath.c_str(), dest))
    {
        AI_LOG_ERROR_EXIT("Failed to loop mount %s", dest.c_str());
        return false;
    }
    mMounted.push_back(dest.c_str());

    dest = mRootfsPath + MOUNT_VENDOR;
    if (!doLoopMount(mVendorPath.c_str(), dest))
    {
        AI_LOG_ERROR_EXIT("Failed to loop mount %s", mVendorPath.c_str());
        return false;
    }
    mMounted.push_back(dest.c_str());

    dest = mRootfsPath + MOUNT_DATA;
    if(!doBindMount(mDataPath, dest))
    {
        AI_LOG_ERROR_EXIT("Failed to bind mount %s->%s", mDataPath.c_str(), dest.c_str());
        return false;
    }
    mMounted.push_back(dest.c_str());

    dest = mRootfsPath + MOUNT_DATA + MOUNT_CACHE;
    if(!doBindMount(mCachePath, dest))
    {
        AI_LOG_ERROR_EXIT("Failed to bind mount %s->%s", mCachePath.c_str(), dest.c_str());
        return false;
    }
    mMounted.push_back(dest.c_str());

    dest = mRootfsPath + MOUNT_CMDLINE;
    if(!doBindFile(mCmdlinePath, dest))
    {
        AI_LOG_ERROR_EXIT("Failed to bind file %s->%s", mCmdlinePath.c_str(), dest.c_str());
        return false;
    }
    mMounted.push_back(dest.c_str());

    for(std::string s : mMounted)
    {
        AI_LOG_INFO("Mounted %s", s.c_str());
    }
    AI_LOG_FN_EXIT();
    return true;
}

bool AndroidRuntimePlugin::doUnmounts()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_INFO("doUnmount complete");
    for(std::vector<std::string>::reverse_iterator it = mMounted.rbegin(); it != mMounted.rend(); it++)
    {
        AI_LOG_INFO("Unmounting %s", it->c_str());
        if(umount2(it->c_str(), UMOUNT_NOFOLLOW) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to unmount %s", it->c_str());
        }
    }
    mMounted.clear();

    AI_LOG_INFO("doUnmount complete");

    AI_LOG_FN_EXIT();

    return true;
}
