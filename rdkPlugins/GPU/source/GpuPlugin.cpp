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
      mUtils(utils)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

unsigned GpuPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::PostStopFlag);
}

// Begin Hook Methods

/**
 *  @brief we use the createRuntime point to create a cgroup and put the
 *  containered process into it.
 *
 *  The amount of memory to assign is read from the plugin's data section in
 *  the bundle config.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @return true if successful, otherwise false.
 */
bool GpuPlugin::createRuntime()
{
    const std::string cgroupDirPath = getGpuCgroupMountPoint();

    // sanity check we have a gpu cgroup dir
    if (cgroupDirPath.empty())
    {
        AI_LOG_ERROR_EXIT("missing gpu cgroup directory");
        return false;
    }

    // get the container pid
    pid_t containerPid = mUtils->getContainerPid();
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
 *  @brief We use the postStop point to remove the cgroup directory
 *  created in the createRuntime phase.
 *
 *  The directory will have the same name as the container id.
 *
 *  @return true if successful, otherwise false.
 */
bool GpuPlugin::postStop()
{
    AI_LOG_FN_ENTRY();

    const std::string cgroupDirPath = getGpuCgroupMountPoint();

    // sanity check we have a gpu cgroup dir
    if (cgroupDirPath.empty())
    {
        AI_LOG_ERROR_EXIT("missing gpu cgroup directory");
        return false;
    }

    // remove the container's gpu cgroup directory
    const std::string cgroupPath = cgroupDirPath + "/" + mUtils->getContainerId();
    if (rmdir(cgroupPath.c_str()) < 0)
    {
        // we could be called at stop time even though the createRuntime hook
        // wasn't called due to an earlier plugin failing ... so don't report
        // an error if the directory didn't exist
        if (errno != ENOENT)
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete gpu cgroup dir '%s'",
                             mUtils->getContainerId().c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

/**
 * @brief Should return the names of the plugins this plugin depends on.
 *
 * This can be used to determine the order in which the plugins should be
 * processed when running hooks.
 *
 * @return Names of the plugins this plugin depends on.
 */
std::vector<std::string> GpuPlugin::getDependencies() const
{
    std::vector<std::string> dependencies;
    const rt_defs_plugins_gpu* pluginConfig = mContainerConfig->rdk_plugins->gpu;

    for (size_t i = 0; i < pluginConfig->depends_on_len; i++)
    {
        dependencies.push_back(pluginConfig->depends_on[i]);
    }

    return dependencies;
}

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
 *  @brief Creates a gpu cgroup for the container and moves the container into
 *  it.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @warning This requires a version of crun with the following PR:
 *  https://github.com/containers/crun/pull/609 to ensure cgroup controllers are
 *  correctly mounted. Without the PR applied, the GPU cgroup is mounted incorrectly,
 *  see https://github.com/containers/crun/issues/625 for more info
 *
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

    // setup the paths for the cgroup, i.e. "/sys/fs/cgroup/gpu/<id>"
    const std::string cgroupPath(cgroupDirPath + "/" + mUtils->getContainerId());

    // create a new cgroup (we're ok with it already existing)
    if ((mkdir(cgroupPath.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create gpu cgroup dir '%s'",
                              mUtils->getContainerId().c_str());
        return false;
    }

    // move the containered pid into the new cgroup
    const std::string procsPath = cgroupPath + "/cgroup.procs";
    if (!mUtils->writeTextFile(procsPath, std::to_string(containerPid),
                               O_CREAT | O_TRUNC, 0700))
    {
        AI_LOG_ERROR_EXIT("failed to put the container '%s' into the cgroup",
                          mUtils->getContainerId().c_str());
        return false;
    }

    // set the gpu memory limit on the container
    const std::string gpulimitPath = cgroupPath + "/gpu.limit_in_bytes";
    if (!mUtils->writeTextFile(gpulimitPath, std::to_string(memoryLimit),
                               O_CREAT | O_TRUNC, 0700))
    {
        AI_LOG_ERROR_EXIT("failed to set the gpu memory limit for container "
                          "'%s'", mUtils->getContainerId().c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End private methods
