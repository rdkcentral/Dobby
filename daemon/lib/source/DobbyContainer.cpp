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
 * File:   DobbyContainer.cpp
 *
 */
#include "DobbyContainer.h"

#include <Logging.h>

#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>


std::mutex DobbyContainer::mIdsLock;
std::bitset<1024> DobbyContainer::mUsedIds;


// -----------------------------------------------------------------------------
/**
 *  @brief Alloc a unique descriptor from the pool.
 *
 *  The descriptor will be a number between 1 and 1024, it will be unique in the
 *  sense that no existing Container object will have the same descriptor.
 *
 *  The descriptors themselves are created in a pseudo random repeating
 *  sequence, which should hopefully avoid getting the same descriptor number
 *  close together.
 *
 *  @return a unique descriptor number between 1 and 1024
 */
int32_t DobbyContainer::allocDescriptor()
{
    static unsigned lfsr = 0x1bcu;
    unsigned bit;

    std::lock_guard<std::mutex> locker(mIdsLock);

    // sanity check we haven't used up all the container ids
    if (mUsedIds.count() >= (1024 - 1))
    {
        AI_LOG_FATAL("consumed all possible container ids");
        return -1;
    }

    // use a fibonacci LFSR to cycle through descriptors rather than just a
    // random number generator to avoid same descriptors being used close
    // together
    do
    {
        // taps: 10 7; feedback polynomial: x^10 + x^7 + 1
        bit  = ((lfsr >> 0) ^ (lfsr >> 3)) & 1;
        lfsr =  (lfsr >> 1) | (bit << 9);

    } while (mUsedIds.test(lfsr) == true);


    // reserve the id and return it
    mUsedIds.set(lfsr);
    return lfsr;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Frees a descriptor created with allocDescriptor.
 *
 *  This puts the given descriptor back in the pool for use again.
 *
 *  @param[in]  cd      the descriptor to release.
 */
void DobbyContainer::freeDescriptor(int32_t cd)
{
    std::lock_guard<std::mutex> locker(mIdsLock);

    if ((cd < 1) || (cd >= (int32_t)mUsedIds.size()) || !mUsedIds.test(cd))
    {
        AI_LOG_FATAL("trying to free an id (%d) that wasn't taken", cd);
        return;
    }

    mUsedIds.reset(cd);
}



DobbyContainer::DobbyContainer(const std::shared_ptr<const DobbyBundle>& _bundle,
                               const std::shared_ptr<const DobbyConfig>& _config,
                               const std::shared_ptr<const DobbyRootfs>& _rootfs)
    : descriptor(allocDescriptor())
    , bundle(_bundle)
    , config(_config)
    , rootfs(_rootfs)
    , containerPid(-1)
    , hasCurseOfDeath(false)
    , state(State::Starting)
    , mRestartOnCrash(false)
    , mRestartCount(0)
{
}

DobbyContainer::DobbyContainer(const std::shared_ptr<const DobbyBundle>& _bundle,
                               const std::shared_ptr<const DobbyConfig>& _config,
                               const std::shared_ptr<const DobbyRootfs>& _rootfs,
                               const std::shared_ptr<const DobbyRdkPluginManager>& _rdkPluginManager)
    : descriptor(allocDescriptor())
    , bundle(_bundle)
    , config(_config)
    , rootfs(_rootfs)
    , rdkPluginManager(_rdkPluginManager)
    , containerPid(-1)
    , hasCurseOfDeath(false)
    , state(State::Starting)
    , mRestartOnCrash(false)
    , mRestartCount(0)
{
}

DobbyContainer::~DobbyContainer()
{
    clearRestartOnCrash();

    if (descriptor >= 0)
        freeDescriptor(descriptor);
}


void DobbyContainer::setRestartOnCrash(const std::list<int>& files_)
{
    AI_LOG_FN_ENTRY();

    // the restart on death shouldn't be set twice
    if (mRestartOnCrash)
    {
        AI_LOG_ERROR_EXIT("restart-on-crash flag already set");
        return;
    }

    // dup the supplied file descriptors to ensure that they don't disappear
    // from underneath us.
    std::list<int>::const_iterator it = files_.begin();
    for (; it != files_.end(); ++it)
    {
        int fd = fcntl(*it, F_DUPFD_CLOEXEC, 3);
        if (fd < 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "F_DUPFD_CLOEXEC failed");
            return;
        }

        mFiles.push_back(fd);
    }

    mRestartCount = 0;
    mLastRestartAttempt = std::chrono::steady_clock::now();

    mRestartOnCrash = true;

    AI_LOG_FN_EXIT();
}

void DobbyContainer::clearRestartOnCrash()
{
    AI_LOG_FN_ENTRY();

    for (int fd : mFiles)
    {
        if ((fd >= 0) && (close(fd) != 0))
        {
            AI_LOG_SYS_ERROR(errno, "failed to close descriptor");
        }
    }

    mFiles.clear();

    mRestartOnCrash = false;

    AI_LOG_FN_EXIT();
}

const std::list<int>& DobbyContainer::files() const
{
    return mFiles;
}

bool DobbyContainer::shouldRestart(int statusCode)
{
    if (!mRestartOnCrash || (statusCode == EXIT_SUCCESS))
    {
        return false;
    }

    // to avoid endless attempts to restart if there is some fatal error, just
    // try respawning 10 times, unless the last respawn was ages (5 minutes) ago

    std::chrono::time_point<std::chrono::steady_clock> now = std::chrono::steady_clock::now();

    if ((now - mLastRestartAttempt) > std::chrono::minutes(5))
    {
        mRestartCount = 0;
    }

    if (++mRestartCount > 10)
    {
        AI_LOG_ERROR("container restart has been attempted 10 times, each has "
                     "failed within the last 5 miuntes so giving up.");
        return false;
    }
    else
    {
        AI_LOG_INFO("container will try and be re-started");
        mLastRestartAttempt = now;
        return true;
    }
}


