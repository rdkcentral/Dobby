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

#include "GpuPlugin.h"

#include <linux/limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <mntent.h>

REGISTER_RDK_PLUGIN(GpuPlugin);

GpuPlugin::GpuPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                     const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                     const std::string &rootfsPath)
    : mName("Gpu"),
      mContainerConfig(containerConfig),
      mUtils(utils),
      mContainerId(mContainerConfig->hostname)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

unsigned GpuPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::PostInstallationFlag |
        IDobbyRdkPlugin::HintFlags::PostHaltFlag);
}

// Begin Hook Methods

/**
 *  @brief we use the postInstallation point to create a cgroup and put the
 *  containered process into it.
 *
 *  The amount of memory to assign is read from the plugin's data section in
 *  the bundle config.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @return true if successful, otherwise false.
 */
bool GpuPlugin::postInstallation()
{
    const std::string cgroupDirPath = getGpuCgroupMountPoint();

    // if we don't have a GPU cgroup controller then nothing to do
    if (cgroupDirPath.empty())
    {
        AI_LOG_ERROR_EXIT("missing gpu cgroup directory");
        return false;
    }

    // get the container pid
    pid_t containerPid = mUtils->getContainerPid(mUtils->getHookStdin());
    if (!containerPid)
    {
        AI_LOG_ERROR_EXIT("couldn't find container pid");
        return false;
    }

    // get memory limit from the config
    if (mContainerConfig->rdk_plugins->gpu->data->memory <= 0)
    {
        AI_LOG_ERROR_EXIT("gpu memory limit must be > 0");
        return false;
    }

    const int memLimit = mContainerConfig->rdk_plugins->gpu->data->memory;

    // setup the gpu memory limit
    setupContainerGpuLimit(cgroupDirPath, containerPid, memLimit);
}

/**
 *  @brief We use the postHalt point to remove the cgroup directory
 *  created in the createRuntime phase.
 *
 *  The directory will have the same name as the container id.
 *
 *  @return true if successful, otherwise false.
 */
bool GpuPlugin::postHalt()
{
    AI_LOG_FN_ENTRY();

    const std::string cgroupDirPath = getGpuCgroupMountPoint();

    // sanity check we have a gpu cgroup dir
    if (cgroupDirPath.empty())
    {
        AI_LOG_WARN("no gpu cgroup directory found");
        return true;
    }

    // remove the container's gpu cgroup directory
    const std::string cgroupPath = cgroupDirPath + "/" + mContainerId.c_str();
    if (rmdir(cgroupPath.c_str()) < 0)
    {
        // we could be called at stop time even though the createRuntime hook
        // wasn't called due to an earlier plugin failing ... so don't report
        // an error if the directory didn't exist
        if (errno != ENOENT)
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete gpu cgroup dir '%s'",
                             mContainerId.c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

// Begin private methods

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to get the mount point of the gpu cgroup filesystem.
 *
 *  This scans the mount table looking for the cgroups mount, if this fails
 *  it's pretty fatal.
 *
 *  This is typically "/sys/fs/cgroup/gpu".
 *
 *  @return a string to the gpu cgroup path.
 */
std::string GpuPlugin::getGpuCgroupMountPoint()
{
    AI_LOG_FN_ENTRY();

    std::string mountPoint;

    // try and open /proc/mounts for scanning the current mount table
    FILE* procMounts = setmntent("/proc/mounts", "r");
    if (procMounts == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open '/proc/mounts'");
        return std::string();
    }

    struct mntent mntBuf;
    struct mntent *mnt;
    char buf[PATH_MAX + 256];

    // loop over all the mounts
    while ((mnt = getmntent_r(procMounts, &mntBuf, buf, sizeof(buf))) != nullptr)
    {
        // skip entries that don't have a mountpoint, type or options
        if (!mnt->mnt_dir || !mnt->mnt_type || !mnt->mnt_opts)
            continue;

        // skip non-cgroup mounts
        if (strcmp(mnt->mnt_type, "cgroup") != 0)
            continue;

        // check for the cgroup type
        char *mntopt = hasmntopt(mnt, "gpu");
        if (!mntopt || strcmp(mntopt, "gpu") != 0)
        {
            continue;
        }

        AI_LOG_DEBUG("found gpu cgroup, mounted @ '%s'", mnt->mnt_dir);

        mountPoint = mnt->mnt_dir;
        break;
    }

    // close /proc/mounts
    endmntent(procMounts);

    AI_LOG_FN_EXIT();
    return mountPoint;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called in the mount namespace of the container
 *
 *  crun mounts the gpu cgroup in the container, but it's the root of the
 *  cgroup tree rather than the cgroup created for the container.
 *
 *  For example this is the mount layout setup by crun:
 *
 *      /sys/fs/cgroup/cpu/<ID>    -> <ROOTFS>/sys/fs/cgroup/freezer
 *      /sys/fs/cgroup/memory/<ID> -> <ROOTFS>/sys/fs/cgroup/memory
 *      /sys/fs/cgroup/gpu         -> <ROOTFS>/sys/fs/cgroup/gpu
 *      ...
 *
 *  We want it to be:
 *
 *      /sys/fs/cgroup/gpu/<ID>    -> <ROOTFS>/sys/fs/cgroup/gpu
 *      ...
 *
 *  So we need to replace the existing gpu mount with the cgroup for this
 *  container.
 *
 *  NB: this is not a security thing, but if we don't do this, the app inside
 *  the container would have to know it's container id to monitor its own usage
 *  usage. This is a problem for apps which expect the cgroup to be mounted for
 *  the container only.
 *
 *  @param[in]  source      The source path of the cgroup.
 *  @param[in]  target      The target mount point.
 *
 *  @return true if successful, otherwise false.
 */
bool GpuPlugin::bindMountGpuCgroup(const std::string &source,
                                   const std::string &target)
{
    AI_LOG_FN_ENTRY();

    // try and do the bind mount
    if (mount(source.c_str(), target.c_str(), nullptr, MS_BIND, nullptr) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to bind mount '%s' to '%s'",
                         source.c_str(), target.c_str());
        return false;
    }

    AI_LOG_DEBUG("bind mounted '%s' to '%s'", source.c_str(), target.c_str());

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a gpu cgroup for the container and moves the container into
 *  it.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @param[in]  cgroupDirPath   Path to the gpu cgroup directory.
 *  @param[in]  containerPid    The pid of the process in the container.
 *  @param[in]  memoryLimit     The memory limit set in the bundle config.
 *
 *  @return true if successful, otherwise false.
 */
bool GpuPlugin::setupContainerGpuLimit(const std::string cgroupDirPath,
                                       pid_t containerPid,
                                       int memoryLimit)
{
    AI_LOG_FN_ENTRY();

    // setup the paths for the bind mount, i.e.
    //   source:   "/sys/fs/cgroup/gpu/<id>"
    //   target:   "/sys/fs/cgroup/gpu"
    const std::string sourcePath(cgroupDirPath + "/" + mContainerId);
    const std::string targetPath(cgroupDirPath);

    // create a new cgroup (we're ok with it already existing)
    if ((mkdir(sourcePath.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create gpu cgroup dir '%s'",
                              mContainerId.c_str());
        return false;
    }

    // move the containered pid into the new cgroup
    const std::string procsPath = sourcePath + "/cgroup.procs";
    if (!mUtils->writeTextFile(procsPath, std::to_string(containerPid),
                               O_CREAT | O_TRUNC, 0700))
    {
        AI_LOG_ERROR_EXIT("failed to put the container '%s' into the cgroup",
                          mContainerId.c_str());
        return false;
    }

    // set the gpu memory limit on the container
    const std::string gpulimitPath = sourcePath + "/gpu.limit_in_bytes";
    if (!mUtils->writeTextFile(gpulimitPath, std::to_string(memoryLimit),
                               O_CREAT | O_TRUNC, 0700))
    {
        AI_LOG_ERROR_EXIT("failed to set the gpu memory limit for container "
                          "'%s'", mContainerId.c_str());
        return false;
    }

    // bind mount the container specfic cgroup into the container
    if (!mUtils->callInNamespace(containerPid, CLONE_NEWNS,
                                 &GpuPlugin::bindMountGpuCgroup,
                                 this, sourcePath, targetPath))
    {
        AI_LOG_ERROR_EXIT("hook failed to enter mount namespace");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End private methods
