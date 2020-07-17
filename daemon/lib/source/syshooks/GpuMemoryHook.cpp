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
 * File:   GpuMemoryHook.h
 *
 * Copyright (C) Sky UK 2016+
 */
#include "GpuMemoryHook.h"
#include <IDobbyEnv.h>
#include <IDobbyUtils.h>

#include <Logging.h>

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>



GpuMemoryHook::GpuMemoryHook(const std::shared_ptr<IDobbyEnv>& env,
                             const std::shared_ptr<IDobbyUtils>& utils)
    : mUtilities(utils)
    , mCgroupDirfd(-1)
    , mCgroupDirPath(env->cgroupMountPath(IDobbyEnv::Cgroup::Gpu))
{
    AI_LOG_FN_ENTRY();

    if (mCgroupDirPath.empty())
    {
        AI_LOG_FATAL_EXIT("no GPU cgroup found!");
        return;
    }

    mCgroupDirfd = open(mCgroupDirPath.c_str(), O_DIRECTORY | O_CLOEXEC);
    if (mCgroupDirfd < 0)
    {
        AI_LOG_FATAL_EXIT("failed to open '%s' directory", mCgroupDirPath.c_str());
        return;
    }

    AI_LOG_FN_EXIT();
}

GpuMemoryHook::~GpuMemoryHook()
{
    AI_LOG_FN_ENTRY();

    if ((mCgroupDirfd >= 0) && (close(mCgroupDirfd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close cgroup dir");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the name of the hook
 *
 */
std::string GpuMemoryHook::hookName() const
{
    return std::string("GpuMemHook");
}

// -----------------------------------------------------------------------------
/**
 *  @brief Hook hints for when to run the network hook
 *
 *  We want to be called at the pre-start and post-stop phase. The preStart is
 *  run qsynchronously as we need to do some bind mounting within the namespace.
 *
 */
unsigned GpuMemoryHook::hookHints() const
{
    return (IDobbySysHook::PreStartAsync |
            IDobbySysHook::PostStopSync);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes the value into the given cgroup file
 *
 *  The cgroup path is made up of the container id and the supplied @a fileName.
 *
 *  The value is converted to a string before being written into the file.
 *
 *  @param[in]  id          The id of the container, used as the directory name
 *                          of the cgroup.
 *  @param[in]  fileName    The name of the file in the cgroup to write to.
 *  @param[in]  value       The value to write.
 *
 *  @return true if successiful otherwise false.
 */
bool GpuMemoryHook::writeCgroupFile(const ContainerId& id,
                                    const std::string& fileName,
                                    size_t value)
{
    const std::string filePath(id.str() + "/" + fileName);
    int fd = openat(mCgroupDirfd, filePath.c_str(), O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '%s'", filePath.c_str());
        return false;
    }

    char buf[64];
    size_t n = sprintf(buf, "%zu\n", value);
    const char* s = buf;

    while (n > 0)
    {
        ssize_t written = TEMP_FAILURE_RETRY(write(fd, s, n));
        if (written < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to write to file");
            break;
        }
        else if (written == 0)
        {
            break;
        }

        s += written;
        n -= written;
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '%s'", filePath.c_str());
    }
    
    return (n == 0);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called in the mount namespace of the container
 *
 *  The runc tool does mount the gpu cgroup in the container, but it's the
 *  root of the cgroup tree rather than the cgroup created for the container.
 *
 *  For example this is the mount layout setup by runc:
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
 *  So we need to umount the existing gpu mount and replace it with the cgroup
 *  for this container.
 *
 *  Nb this is not a security thing, but if we don't do this the app inside
 *  the container would have to know it's container id so it could monitor
 *  it's own usage, this is a problem for existing sky apps (like the EPG)
 *  which expects the cgroup to mounted for the container only.
 *
 *  @param[in]  source      The source path of the cgroup.
 *  @param[in]  target      The target mount point.
 *
 */
void GpuMemoryHook::bindMountGpuCgroup(const std::string& source,
                                       const std::string& target)
{
    AI_LOG_FN_ENTRY();

    // now try and do the bind mount
    if (mount(source.c_str(), target.c_str(), nullptr, MS_BIND, nullptr) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to bind mount '%s' to '%s'",
                         source.c_str(), target.c_str());
    }
    else
    {
        AI_LOG_INFO("bind mounted '%s' to '%s'", source.c_str(),
                    target.c_str());
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a gpu cgroup for the container and moves the container into
 *  it.
 *
 *  The amount of memory to assign is retrieved from the config object.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  containerPid    The pid of the process in the container.
 *  @param[in]  config          The container config.
 *
 *  @return true if successiful otherwise false.
 */
bool GpuMemoryHook::setupContainerGpuLimit(const ContainerId& id,
                                           pid_t containerPid,
                                           const std::shared_ptr<const DobbyConfig>& config)
{
    AI_LOG_FN_ENTRY();

    // sanity check we have a gpu cgroup dir
    if (mCgroupDirfd < 0)
    {
        AI_LOG_ERROR_EXIT("missing gpu cgroup dirfd");
        return false;
    }

    // create a new cgroup (we're 'sort of' ok with it already existing)
    if ((mkdirat(mCgroupDirfd, id.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create gpu cgroup dir '%s'",
                              id.c_str());
        return false;
    }

    // move the container'ed pid into the new cgroup
    if (!writeCgroupFile(id, "cgroup.procs", containerPid))
    {
        AI_LOG_ERROR_EXIT("failed to put the container '%s' into the cgroup",
                          id.c_str());
        return false;
    }

    // set the gpu memory limit on the container
    if (!writeCgroupFile(id, "gpu.limit_in_bytes", config->gpuMemLimit()))
    {
        AI_LOG_ERROR_EXIT("failed to set the gpu memory limit for container "
                          "'%s'", id.c_str());
        return false;
    }

    // setup the paths for the bind mount, i.e.
    //   source:   "/sys/fs/cgroup/gpu/<id>"
    //   target:   "/sys/fs/cgroup/gpu"
    const std::string sourcePath(mCgroupDirPath + "/" + id.str());
    const std::string targetPath(mCgroupDirPath);

    // bind mount the container specfic cgroup into the container
    std::function<void()> bindMounter = std::bind(&GpuMemoryHook::bindMountGpuCgroup,
                                                  this, sourcePath, targetPath);
    if (!mUtilities->callInNamespace(containerPid, CLONE_NEWNS, bindMounter))
    {
        AI_LOG_ERROR_EXIT("hook failed to enter mount namespace");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called in the mount namespace of the container
 *
 *  This method unmounts the gpu cgroup within the container, this is not a
 *  requirement but a nicety.
 *
 *
 *  @param[in]  mountPoint  The mount point to unmount.
 *
 */
void GpuMemoryHook::unmountGpuCgroup(const std::string& mountPoint)
{
    AI_LOG_FN_ENTRY();

    if (umount2(mountPoint.c_str(), UMOUNT_NOFOLLOW) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to unmount '%s'", mountPoint.c_str());
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Prestart hook we use this point to create a cgroup and put the
 *  containered process into it.
 *
 *  The amount of memory to assign is retrieved from the config object.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  containerPid    The pid of the process in the container.
 *  @param[in]  config          The container config.
 *  @param[in]  rootfs          The path to the container rootfs.
 *
 *  @return true if successful otherwise false.
 */
bool GpuMemoryHook::preStart(const ContainerId& id,
                             pid_t containerPid,
                             const std::shared_ptr<const DobbyConfig>& config,
                             const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    if (config->gpuEnabled())
    {
        return setupContainerGpuLimit(id, containerPid, config);
    }
    else
    {
        // if the graphics devices aren't enabled then we don't need to do
        // anything, however just so the container is sane we un-mount the gpu
        // cgroup from the container if it was added by runc

        std::function<void()> unMounter = std::bind(&GpuMemoryHook::unmountGpuCgroup,
                                                    this, mCgroupDirPath);
        if (!mUtilities->callInNamespace(containerPid, CLONE_NEWNS, unMounter))
        {
            AI_LOG_ERROR_EXIT("hook failed to enter mount namespace");
            return false;
        }

        return true;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Poststop hook, we use this point to remove the cgroup directory
 *  created in the pre start phase.
 *
 *  The directory will have the same name as the container id.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  config          The container config.
 *  @param[in]  rootfs          The path to the container rootfs.
 *
 *  @return true if successful otherwise false.
 */
bool GpuMemoryHook::postStop(const ContainerId& id,
                             const std::shared_ptr<const DobbyConfig>& config,
                             const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    AI_LOG_FN_ENTRY();

    // sanity check we have a gpu cgroup dir
    if (mCgroupDirfd < 0)
    {
        AI_LOG_ERROR("missing gpu cgroup dirfd");
    }
    else if (unlinkat(mCgroupDirfd, id.c_str(), AT_REMOVEDIR) != 0)
    {
        // we could be called at stop time even though the pre-start hook wasn't
        // called due to an earlier prestart hook failing ... so don't report
        // an error if the directory didn't exist
        if (errno != ENOENT)
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete gpu cgroup dir '%s'",
                             id.c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}
