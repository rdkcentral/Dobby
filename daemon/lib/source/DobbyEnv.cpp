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
 * File:   DobbyEnv.cpp
 *
 */
#include "DobbyEnv.h"

#include <FileUtilities.h>
#include <Logging.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <limits.h>



DobbyEnv::DobbyEnv(const std::shared_ptr<const IDobbySettings>& settings)
    : mWorkspacePath(settings->workspaceDir())
    , mFlashMountPath(settings->persistentDir())
    , mPluginsWorkspacePath(mWorkspacePath + "/dobby/plugins")
    , mCgroupMountPaths(getCgroupMountPoints())
    , mPlatformIdent(getPlatformIdent())
{
    // create a directory within the top level workspace dir for the plugins
    // to use exclusively
    if (!AICommon::mkdirRecursive(mPluginsWorkspacePath, 0755))
    {
        AI_LOG_SYS_FATAL(errno, "failed to create workspace dir '%s'",
                         mPluginsWorkspacePath.c_str());
    }
}

std::string DobbyEnv::workspaceMountPath() const
{
    return mWorkspacePath;
}

std::string DobbyEnv::flashMountPath() const
{
    return mFlashMountPath;
}

std::string DobbyEnv::pluginsWorkspacePath() const
{
    return  mPluginsWorkspacePath;
}

std::string DobbyEnv::cgroupMountPath(Cgroup cgroup) const
{
    std::map<IDobbyEnv::Cgroup, std::string>::const_iterator it =
        mCgroupMountPaths.find(cgroup);

    if (it == mCgroupMountPaths.end())
        return std::string();
    else
        return it->second;
}

uint16_t DobbyEnv::platformIdent() const
{
    return mPlatformIdent;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to get the STB platform identifier bytes
 *
 *  The bytes should be set in the AI_PLATFORM_IDENT environment variable, if
 *  they aren't, or the env var is invalid then we return 0x0000 for the
 *  platform.
 *
 *  @return the two platform identifier bytes
 */
uint16_t DobbyEnv::getPlatformIdent()
{
    AI_LOG_FN_ENTRY();

    // check for the platform environment var
    const char* platformIdent = getenv("AI_PLATFORM_IDENT");
    if ((platformIdent == nullptr) || (platformIdent[0] == '\0'))
    {
#if !defined(RDK)
        AI_LOG_ERROR("missing AI_PLATFORM_IDENT environment var");
#endif
        AI_LOG_FN_EXIT();
        return 0x0000;
    }

    // sanity check we have at least 4 characters and the first four are hex
    // digits
    size_t len = strlen(platformIdent);
    if ((len < 4) || !isxdigit(platformIdent[0]) || !isxdigit(platformIdent[1]) ||
                     !isxdigit(platformIdent[2]) || !isxdigit(platformIdent[3]))
    {
        AI_LOG_ERROR_EXIT("the AI_PLATFORM_IDENT environment var ('%s') is"
                          " invalid", platformIdent);
        return 0x0000;
    }

    AI_LOG_FN_EXIT();
    const std::string ident(platformIdent, platformIdent + 4);
    return static_cast<uint16_t>(strtoul(ident.c_str(), nullptr, 16));
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to get the mount points of the cgroup filesystems
 *
 *  This scans the mount table looking for the cgroups mounts, if this fails
 *  it's pretty fatal.
 *
 *  This is typically the name of the cgroup prefixed with "/sys/fs/cgroup"
 *
 *  @return a map of cgroup type to path
 */
std::map<IDobbyEnv::Cgroup, std::string> DobbyEnv::getCgroupMountPoints()
{
    AI_LOG_FN_ENTRY();

    std::map<IDobbyEnv::Cgroup, std::string> mounts;

    // map of cgroup name to type
    const std::map<std::string, IDobbyEnv::Cgroup> cgroupNames =
    {
        {   "freezer",  IDobbyEnv::Cgroup::Freezer  },
        {   "memory",   IDobbyEnv::Cgroup::Memory   },
        {   "cpu",      IDobbyEnv::Cgroup::Cpu      },
        {   "cpuacct",  IDobbyEnv::Cgroup::CpuAcct  },
        {   "cpuset",   IDobbyEnv::Cgroup::CpuSet   },
        {   "devices",  IDobbyEnv::Cgroup::Devices  },
        {   "gpu",      IDobbyEnv::Cgroup::Gpu      },
        {   "net_cls",  IDobbyEnv::Cgroup::NetCls   },
        {   "blkio",    IDobbyEnv::Cgroup::Blkio,   },
        {   "ion",      IDobbyEnv::Cgroup::Ion      },
    };

    // try and open /proc/mounts for scanning the current mount table
    FILE* procMounts = setmntent("/proc/mounts", "r");
    if (procMounts == nullptr)
    {
        AI_LOG_SYS_FATAL_EXIT(errno, "failed to open '/proc/mounts' file");
        return std::map<IDobbyEnv::Cgroup, std::string>();
    }

    // loop over all the mounts
    struct mntent mntBuf;
    struct mntent* mnt;
    char buf[PATH_MAX + 256];

    while ((mnt = getmntent_r(procMounts, &mntBuf, buf, sizeof(buf))) != nullptr)
    {
        // skip entries that don't have a mountpount, type or options
        if (!mnt->mnt_type || !mnt->mnt_dir || !mnt->mnt_opts)
            continue;

        // skip non-cgroup mounts
        if (strcmp(mnt->mnt_type, "cgroup") != 0)
            continue;

        // check for the cgroup type
        for (const std::pair<const std::string, IDobbyEnv::Cgroup> cgroup : cgroupNames)
        {
            char* mntopt = hasmntopt(mnt, cgroup.first.c_str());
            if (!mntopt)
                continue;

            if (strcmp(mntopt, cgroup.first.c_str()) != 0)
                continue;

            AI_LOG_INFO("found cgroup '%s' mounted @ '%s'",
                        cgroup.first.c_str(), mnt->mnt_dir);

            mounts[cgroup.second] = mnt->mnt_dir;
            break;
        }
    }

    endmntent(procMounts);

    AI_LOG_FN_EXIT();
    return mounts;
}

