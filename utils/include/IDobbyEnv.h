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
 * File:   IDobbyEnv.h
 *
 */
#ifndef IDOBBYENV_H
#define IDOBBYENV_H

#include <cstdint>
#include <string>


// -----------------------------------------------------------------------------
/**
 *  @class IDobbyEnv
 *  @brief Interface that exports the environment of the daemon to plugins
 *
 *  A const instance of this interface is given to plugins when they are
 *  installed, it provides some basic information about the location of mount
 *  points and the system.
 *
 *  The interface is only expected to contain static values, it is not expected
 *  that values returned via getters will change over the lifetime of the
 *  object.
 *
 *
 */
class IDobbyEnv
{
public:
    virtual ~IDobbyEnv() = default;

public:

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the absolute AI workspace mount point path
     *
     *  This is the tmpfs mount used by all AI code as a place to store
     *  non-persistent files.
     *
     *  The path is typically "/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE"
     *
     */
    virtual std::string workspaceMountPath() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the absolute path to the AI area on flash
     *
     *  This is the flash mount used to store things like package widget files
     *  and any other sort of persistent data.
     *
     *  The path changes depending on the platform, but the following is
     *  typical "/mnt/nds/dev_17/part_0/appmanager"
     *
     */
    virtual std::string flashMountPath() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the path to directory that plugins can write to
     *
     *  This is non-persistent storage and is just a subdirectory of the
     *  workspace.  Plugins should use this to store any temporary files, mount
     *  points, etc.
     *
     *  The path is typically "/mnt/nds/tmpfs/APPLICATIONS_WORKSPACE/plugins"
     *
     */
    virtual std::string pluginsWorkspacePath() const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the two byte platform identification number
     *
     *  The following list the platforms at time of writing, for a complete list
     *  refer to https://www.stb.bskyb.com/confluence/display/SKYQ/Sky+Q+Hardware
     *
     *      32B0  :  Falcon (broadcom 7445)
     *      32B1  :  FalconV2 UK (ST 419 Gateway)
     *      32B2  :  Titan (Broadcom 7278 Gateway)
     *      32C0  :  X-Wing (ST 412 Gateway)
     *      32C1  :  HIP Box (SKYH412 X-Wing)
     *      32D0  :  MR Box (ST 412 IP-Client)
     *      7D67  :  Amidala (ST 418 Gateway Satellite)
     *      3400  :  Amidala (ST 418 Gateway Cable)
     *      6763  :  Amidala (ST 418 GW Satellite & Cable)
     *      32C2  :  AX2 m-star 64bit platform
     *
     */
    virtual uint16_t platformIdent() const = 0;


    enum class Cgroup
        { Freezer, Memory, Cpu, CpuAcct, CpuSet, Devices, Gpu, NetCls, Blkio };

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the absolute path to the cgroup mount point for the
     *  given cgroup
     *
     *  This is typically "/sys/fs/cgroup/<cgroup>"
     *
     */
    virtual std::string cgroupMountPath(Cgroup cgroup) const = 0;

};


#endif // !defined(IDOBBYENV_H)
