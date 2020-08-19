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
/*
 * File:   LocalTimeHook.cpp
 *
 */
#include "LocalTimeHook.h"

#include <Logging.h>

#include <errno.h>
#include <unistd.h>
#include <limits.h>



LocalTimeHook::LocalTimeHook(const std::shared_ptr<IDobbyUtils>& utils)
    : mUtilities(utils)
{
    AI_LOG_FN_ENTRY();

    // get the real path to the correct local time zone
    char pathBuf[PATH_MAX];
    ssize_t len = readlink("/etc/localtime", pathBuf, sizeof(pathBuf));
    if (len <= 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "readlink failed on '/etc/localtime'");
        return;
    }

    // store the real path
    mTimeZonePath.assign(pathBuf, len);

    AI_LOG_INFO("/etc/localtime symlinked to '%s'", mTimeZonePath.c_str());

    AI_LOG_FN_EXIT();
}

LocalTimeHook::~LocalTimeHook()
{
    AI_LOG_FN_ENTRY();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the name of the hook
 *
 */
std::string LocalTimeHook::hookName() const
{
    return std::string("LocalTimeHook");
}

// -----------------------------------------------------------------------------
/**
 *  @brief Hook hints for when to run this hook
 *
 *
 */
unsigned LocalTimeHook::hookHints() const
{
    return IDobbySysHook::PostConstructionSync;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Post-construction hook
 *
 *  All we need to do create symlink in the container rootfs to the real time
 *  zone file - matching the /etc/localtime entry outside the container.
 *
 *  @param[in]  id              The id of the container.
 *  @param[in]  startupState    The start-up state, used add the bind mount to
 *                              th container config.
 *  @param[in]  config          The container config, ignored.
 *  @param[in]  rootfs          The rootfs of the container.
 *
 *  @return true on success, false on failure.
 */
bool LocalTimeHook::postConstruction(const ContainerId& id,
                                     const std::shared_ptr<IDobbyStartState>& startupState,
                                     const std::shared_ptr<const DobbyConfig>& config,
                                     const std::shared_ptr<const DobbyRootfs>& rootfs)
{
    AI_LOG_FN_ENTRY();

    if (mTimeZonePath.empty())
    {
        AI_LOG_WARN("missing real timezone file path");
    }
    else if (symlinkat(mTimeZonePath.c_str(), rootfs->dirFd(), "etc/localtime") != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create /etc/localtime symlink");
    }

    AI_LOG_FN_EXIT();
    return true;
}

