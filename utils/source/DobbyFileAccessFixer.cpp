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
 * File:   DobbyFileAccessFixer.cpp
 *
 */
#include "DobbyFileAccessFixer.h"

#include <Logging.h>

#include <string>
#include <fstream>

#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <grp.h>

DobbyFileAccessFixer::DobbyFileAccessFixer()
{
}

DobbyFileAccessFixer::~DobbyFileAccessFixer()
{
}

bool DobbyFileAccessFixer::fixIt() const
{
    AI_LOG_FN_ENTRY();

#if !defined(RDK)
    fixDobbyInitPerms();
    fixOptRuntimePerms();
    fixGfxDriverPerms();
    fixCoreDumpFilter();
#endif

    AI_LOG_FN_EXIT();
    return true;
}

void DobbyFileAccessFixer::chmodFile(const char* filePath,
                                     mode_t oldPerms,
                                     mode_t newPerms)
{
    if (chmod(filePath, newPerms) == 0)
    {
        AI_LOG_INFO("fixed perms on '%s' to 0%03o from 0%03o", filePath,
                    newPerms, oldPerms);
    }
    else
    {
        AI_LOG_SYS_ERROR(errno, "failed to change file perms on '%s' from 0%03o"
                         " to 0%03o", filePath, oldPerms, newPerms);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Fixes the access perms on /opt/libexec/DobbyInit
 *  @ref NGDEV-65250
 *
 *  DobbyInit needs to be executable by everyone as it's the init process of
 *  all containers.
 *
 */
bool DobbyFileAccessFixer::fixDobbyInitPerms() const
{
    AI_LOG_FN_ENTRY();

    const std::string initProcessPath("/opt/libexec/DobbyInit");

    struct stat buf;
    if (stat(initProcessPath.c_str(), &buf) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to get details of '%s'",
                              initProcessPath.c_str());
        return false;
    }

    if ((buf.st_mode & 0777) != 0555)
    {
        chmodFile(initProcessPath.c_str(), (buf.st_mode & 07777), 0555);
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback from the nftw() function for the directory walk
 *
 *  This is called for every entry in the /opt/runtimes dir, it will set the
 *  dirs and executable file perms to 0555 and ordinary files have 0444.
 *
 *  @param[in]  filePath        The path to the file or directory
 *  @param[in]  statBuf         The struct stat of the file or directory
 *  @param[in]  typeFlag        The type of entry (file, dir, symlink, etc)
 *  @param[in]  ftwbuf          Ignored
 *
 *  @return always returns 0 to keep the walk going.
 */
int DobbyFileAccessFixer::fixRuntimePerms(const char* filePath,
                                          const struct stat* statBuf,
                                          int typeFlag,
                                          struct FTW* ftwbuf)
{
    // sanity check
    if (!filePath || !statBuf)
    {
        AI_LOG_ERROR("invalid filePath or statBuf");
        return 0;
    }

    // skip the '.' and '..' entries
    if ((filePath[0] == '.') && ((filePath[1] == '\0') ||
                                 ((filePath[1] == '.') && (filePath[2] == '\0'))))
    {
        return 0;
    }

    // process the entry
    switch (typeFlag)
    {
        case FTW_D:
            // fix directory permisions
            if ((statBuf->st_mode & 0777) != 0555)
            {
                chmodFile(filePath, (statBuf->st_mode & 07777), 0555);
            }
            break;

        case FTW_F:
            // fix the file perms, anything currently marked as executable
            // retains that but for all users, everything else is at least
            // readable by everyone
            if (statBuf->st_mode & 0111)
            {
                // executable file
                if ((statBuf->st_mode & 0777) != 0555)
                {
                    chmodFile(filePath, (statBuf->st_mode & 07777), 0555);
                }
            }
            else
            {
                // ordinary file
                if ((statBuf->st_mode & 0777) != 0444)
                {
                    chmodFile(filePath, (statBuf->st_mode & 07777), 0444);
                }
            }
            break;

        case FTW_SL:
            // ignore symlinks
            break;

        default:
            AI_LOG_ERROR("Un-expected file type (%d) found with name '%s'",
                         typeFlag, filePath);
            break;
    };
    
    return 0;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Fixes the access perms on everything in /opt/runtimes
 *  @ref NGDEV-65250
 *
 *  Everything in here needs to be readable by everyone, in addition anything
 *  marked as executable needs to be executable by everyone.
 *
 */
bool DobbyFileAccessFixer::fixOptRuntimePerms() const
{
    AI_LOG_FN_ENTRY();

    // Recurse throug every entry in the the /opt/runtime dir
    if (nftw("/opt/runtimes", fixRuntimePerms, 128, FTW_PHYS) != 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to walk '/opt/runtimes' dir");
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Fixes the perms on the opengl dev nodes
 *  @ref NGDEV-60179
 *
 *  The opengl dev nodes for both the ST and Broadcom currently have perms
 *  that don't allow un-privilaged apps to access them.
 *
 *  This code walks through them all and changes the access perms to allow
 *  'others' to read and write.  The preferred solution is to put those nodes
 *  into a 'graphics' group and run the apps with that as a supplementary
 *  group option.
 *
 *
 */
bool DobbyFileAccessFixer::fixGfxDriverPerms() const
{
    AI_LOG_FN_ENTRY();


    // get the GID number for the group "NDS_GFX", if the opengl dev nodes
    // belong to that group then don't reset their perms
    struct group grp, *result = nullptr;
    char groupBuf[512];
    gid_t ndsGfxGid = 0;

    if (getgrnam_r("NDS_GFX", &grp, groupBuf, sizeof(groupBuf), &result) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to get gid of \"NDS_GFX\" group");
    }
    else if (result != nullptr)
    {
        ndsGfxGid = result->gr_gid;
    }


    const char* filesToFix[] =
    {
        // for ST platforms we need to map in the following
        //   /dev/mali
        //   /dev/xeglhelper
        "/dev/mali",
        "/dev/xeglhelper",

        // for broadcom platforms we need to map in the following
        //   /dev/nds/opengl0
        //   /dev/nds/xeglstreamX   ( where X => { 0 : 11 } )
        "/dev/nds",
        "/dev/nds/opengl0",
        "/dev/nds/xeglstream0",
        "/dev/nds/xeglstream1",
        "/dev/nds/xeglstream2",
        "/dev/nds/xeglstream3",
        "/dev/nds/xeglstream4",
        "/dev/nds/xeglstream5",
        "/dev/nds/xeglstream6",
        "/dev/nds/xeglstream7",
        "/dev/nds/xeglstream8",
        "/dev/nds/xeglstream9",
        "/dev/nds/xeglstream10",
        "/dev/nds/xeglstream11",

        // for broadcom titan platforms we need to map the following
        "/dev/nexus",
        "/dev/bcm_moksha_loader",
        "/dev/dri/card0",
    };

    for (size_t i = 0; i < (sizeof(filesToFix) / sizeof(filesToFix[0])); i++)
    {
        const char* filePath = filesToFix[i];
        struct stat buf;

        // get the current stat, skip if the file/dir doesn't exist
        if (stat(filePath, &buf) != 0)
            continue;

        // skip the modification if the file belongs to the NDS_GFX group
        if ((ndsGfxGid > 0) && (buf.st_gid == ndsGfxGid))
            continue;

        // if a directory ensure it's readable by everyone
        if (S_ISDIR(buf.st_mode))
        {
            if ((buf.st_mode & 0007) != 0005)
            {
                chmodFile(filePath, (buf.st_mode & 0777),
                          ((buf.st_mode & 0770) | 0005));
            }
        }

        // if a dev node then change to make it readable and writeable by
        // everyone
        else if (S_ISCHR(buf.st_mode))
        {
            if ((buf.st_mode & 0007) != 0006)
            {
                chmodFile(filePath, (buf.st_mode & 0777),
                          ((buf.st_mode & 0770) | 0006));
            }
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Fixes the core pattern filter
 *  @ref NGDEV-123877
 *
 *
 */
bool DobbyFileAccessFixer::fixCoreDumpFilter() const
{
    AI_LOG_FN_ENTRY();

    // read the coredump filter which is a string representing
    // a hex number
    std::fstream coreDumpFilterFile("/proc/self/coredump_filter");
    std::string coreDumpFilter;
    coreDumpFilterFile >> coreDumpFilter;
    // convert the hex string to int
    char *endptr;
    int coreDumpFilterAsInt = static_cast<int>(strtol(coreDumpFilter.c_str(), &endptr, 16));
    if(coreDumpFilter.c_str() != endptr)
    {
        // set the flag to dump ELF headers when generating coredumps
        coreDumpFilterAsInt |= (1 << 4);
        // save back to /proc/self/coredump_filter
        coreDumpFilterFile << coreDumpFilterAsInt;
    }
    else
    {
        AI_LOG_ERROR("Could not change coredump filter value");
        return false;
    }
    coreDumpFilterFile.close();

    AI_LOG_FN_EXIT();
    return true;
}
