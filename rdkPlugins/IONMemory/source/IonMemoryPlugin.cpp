/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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

#include "IonMemoryPlugin.h"

#include <regex>
#include <cinttypes>

#include <dirent.h>
#include <fcntl.h>
#include <mntent.h>
#include <unistd.h>
#include <linux/limits.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>

REGISTER_RDK_PLUGIN(IonMemoryPlugin);

IonMemoryPlugin::IonMemoryPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                                 const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                                 const std::string &rootfsPath)
    : mName("IonMemory"),
      mContainerConfig(containerConfig),
      mUtils(utils),
      mRootfsPath(rootfsPath),
      mPluginData(nullptr)
{
    AI_LOG_FN_ENTRY();

    if (mContainerConfig == nullptr ||
        mContainerConfig->rdk_plugins->ionmemory == nullptr ||
        mContainerConfig->rdk_plugins->ionmemory->data == nullptr)
    {
        mValid = false;
    }
    else
    {
        mPluginData = mContainerConfig->rdk_plugins->ionmemory->data;
        mValid = true;
    }

    AI_LOG_FN_EXIT();
}

/**
 * @brief Set the bit flags for which hooks we're going to use
 *
 */
unsigned IonMemoryPlugin::hookHints() const
{
    return (
        IDobbyRdkPlugin::HintFlags::CreateRuntimeFlag |
        IDobbyRdkPlugin::HintFlags::PostStopFlag);
}

// Begin Hook Methods

/**
 * @brief OCI Hook - Run in host namespace. We use this point to create a cgroup and put the
 *  containered process into it.
 *
 *  We also set any limits from the plugin JSON data provided.
 *
 *  The cgroup is given the same name as the container.
 *
 * @return True on success, false on failure.
 */
bool IonMemoryPlugin::createRuntime()
{
    AI_LOG_FN_ENTRY();

    if (!mValid)
    {
        AI_LOG_ERROR_EXIT("Invalid container config");
        return false;
    }

    const std::string cgroupDirPath = findIonCGroupMountPoint();

    // sanity check we have an ION cgroup dir
    if (cgroupDirPath.empty())
    {
        AI_LOG_ERROR_EXIT("missing cgroup directory");
        return false;
    }

    // get the container pid
    pid_t containerPid = mUtils->getContainerPid();
    if (!containerPid)
    {
        AI_LOG_ERROR_EXIT("couldn't find container pid");
        return false;
    }

    // get the default limit and heap limits
    const uint64_t defaultLimitValue = mPluginData->default_limit_present ? mPluginData->default_limit : UINT64_MAX;
    std::map<std::string, uint64_t> heapLimits;
    for (size_t i = 0; i < mPluginData->heaps_len; i++)
    {
        const rt_defs_plugins_ion_memory_data_heaps_element *heap = mPluginData->heaps[i];
        const char *heapName = heap->name;
        const uint64_t heapLimit = heap->limit;

        heapLimits[heapName] = heapLimit;
    }

    // finally apply the limits
    return setupContainerIonLimits(cgroupDirPath, containerPid, heapLimits, defaultLimitValue);

    AI_LOG_FN_EXIT();
    return true;
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
bool IonMemoryPlugin::postStop()
{
    AI_LOG_FN_ENTRY();

    const std::string cgroupDirPath = findIonCGroupMountPoint();

    // sanity check we have a cgroup dir
    if (cgroupDirPath.empty())
    {
        AI_LOG_ERROR_EXIT("missing cgroup directory");
        return false;
    }

    // remove the container's cgroup directory
    const std::string cgroupPath = cgroupDirPath + "/" + mUtils->getContainerId();
    if (rmdir(cgroupPath.c_str()) < 0)
    {
        // we could be called at stop time even though the createRuntime hook
        // wasn't called due to an earlier plugin failing ... so don't report
        // an error if the directory didn't exist
        if (errno != ENOENT)
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete cgroup dir '%s'",
                             mUtils->getContainerId().c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// End hook methods

// Begin private methods

//
/**
 *  @brief Attempts to get the mount points of the ION cgroup filesystem.
 *
 *  This scans the mount table looking for the cgroups mounts. This is typically
 *  the name of the cgroup prefixed with "/sys/fs/cgroup"
 *
 *  @return path to the ION cgroup mount, or empty string if failed to find it.
 */
std::string IonMemoryPlugin::findIonCGroupMountPoint() const
{
    AI_LOG_FN_ENTRY();

    // try and open /proc/mounts for scanning the current mount table
    FILE *procMounts = setmntent("/proc/mounts", "r");
    if (procMounts == nullptr)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open '/proc/mounts' file");
        return std::string();
    }

    // loop over all the mounts
    struct mntent mntBuf;
    struct mntent *mnt;
    char buf[PATH_MAX + 256];
    std::string path;

    while ((mnt = getmntent_r(procMounts, &mntBuf, buf, sizeof(buf))) != nullptr)
    {
        // skip entries that don't have a mountpount, type or options
        if (!mnt->mnt_type || !mnt->mnt_dir || !mnt->mnt_opts)
            continue;

        // skip non-cgroup mounts
        if (strcmp(mnt->mnt_type, "cgroup") != 0)
            continue;

        // check if the ion cgroup
        char *mntopt = hasmntopt(mnt, "ion");
        if (!mntopt || (strcmp(mntopt, "ion") != 0))
            continue;

        AI_LOG_INFO("found ION cgroup mounted @ '%s'", mnt->mnt_dir);

        path = mnt->mnt_dir;
        break;
    }

    endmntent(procMounts);

    AI_LOG_FN_EXIT();
    return path;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a ion cgroup for the container and moves the container into
 *  it.
 *
 *  The amount of memory to assign is retrieved from the config object.
 *
 *  The cgroup is given the same name as the container.
 *
 *  @warning This requires a version of crun with the following PR:
 *  https://github.com/containers/crun/pull/609 to ensure cgroup controllers are
 *  correctly mounted. Without the PR applied, the ION cgroup is mounted incorrectly,
 *  see https://github.com/containers/crun/issues/625 for more info
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  containerPid    The pid of the process in the container.
 *  @param[in]  heapLimits      Map of the heap name to its limits.
 *  @param[in]  defaultLimit    The default limit to set on a heap if not in the
 *                              heapLimits map.
 *
 *  @return true if successful otherwise false.
 */
bool IonMemoryPlugin::setupContainerIonLimits(const std::string &cGroupDirPath,
                                              pid_t containerPid,
                                              const std::map<std::string, uint64_t> &heapLimits,
                                              uint64_t defaultLimit)
{
    const std::string containerId = mUtils->getContainerId();
    // setup the paths for the cgroup, i.e. "/sys/fs/cgroup/ion/<id>"
    const std::string cgroupPath(cGroupDirPath + "/" + containerId);

    // create a new cgroup (we're 'sort of' ok with it already existing)
    if ((mkdir(cgroupPath.c_str(), 0755) != 0) && (errno != EEXIST))
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create gpu cgroup dir '%s'",
                              containerId.c_str());
        return false;
    }

    // move the containered pid into the new cgroup
    const std::string procsPath = cgroupPath + "/cgroup.procs";
    if (!mUtils->writeTextFile(procsPath, std::to_string(containerPid),
                               O_CREAT | O_TRUNC, 0700))
    {
        AI_LOG_ERROR_EXIT("failed to put the container '%s' into the cgroup",
                          containerId.c_str());
        return false;
    }

    // open the directory to iterate through all the heaps
    int dirFd = open(cgroupPath.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (dirFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to re-open the ion cgroup dir?");
        return false;
    }

    DIR *dir = fdopendir(dirFd);
    if (!dir)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to re-open the ion cgroup dir?");
        close(dirFd);
        return false;
    }

    // loop through all the heaps and set either the default limit or the
    // individual heap limit
    const std::regex limitRegex(R"regex((^ion\.)(\w+)(\.limit_in_bytes$))regex");

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        // only care about sysfs files
        if (entry->d_type != DT_REG)
            continue;

        // check if it is a heap's limit file
        std::cmatch matches;
        if (std::regex_match(entry->d_name, matches, limitRegex) &&
            (matches.size() == 4))
        {
            const std::string heapName = matches.str(2);

            uint64_t limit = defaultLimit;

            // get the limit for this heap
            auto it = heapLimits.find(heapName);
            if (it != heapLimits.end())
                limit = it->second;

            if (limit == UINT64_MAX)
            {
                AI_LOG_INFO("setting no limit on ION heap '%s' for container '%s'",
                            heapName.c_str(), containerId.c_str());
            }
            else
            {
                AI_LOG_INFO("setting ion heap '%s' limit to %" PRIu64 " for container '%s'",
                            heapName.c_str(), limit / 1024, containerId.c_str());
            }

            // set the ION heap memory limit on the container
            const std::string filePath = cgroupPath + "/" + entry->d_name;
            if (!mUtils->writeTextFile(filePath, std::to_string(limit),
                                       O_CREAT | O_TRUNC, 0700))
            {
                AI_LOG_ERROR("failed to set the ion heap '%s' memory limit for container "
                             "'%s'",
                             heapName.c_str(), containerId.c_str());
                closedir(dir);
                return false;
            }
        }
    }

    // clean the dir iterator
    closedir(dir);

    AI_LOG_FN_EXIT();
    return true;
}
