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
 * File:   EthanLogLoop.cpp
 *
 */

#include "EthanLogLoop.h"
#include "EthanLogClient.h"

#include <Logging.h>

#include <systemd/sd-event.h>
#include <systemd/sd-journal.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>



EthanLogLoop::EthanLogLoop(const std::string& memCgroupMountPoint)
    : mMemCgroupMountPoint(memCgroupMountPoint)
    , mEventFd(-1)
{
    // create the eventfd to wake the event loop
    mEventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (mEventFd < 0)
    {
        AI_LOG_SYS_FATAL(errno, "failed to eventfd for thread");
    }

    // start the thread that processes all log pipes
    mThread = std::thread(&EthanLogLoop::eventLoop, this);
    if (!mThread.joinable())
    {
        AI_LOG_FATAL("failed to create logging thread");
    }
}

EthanLogLoop::~EthanLogLoop()
{
    // push a quit event onto the queue
    {
        std::lock_guard<std::mutex> locker(mLock);
        mEvents.emplace_back(TerminateEvent());
    }

    // wake the event loop thread
    wakeLoop();

    // wait for the thread to finish
    if (mThread.joinable())
    {
        mThread.join();
    }

    // close the eventfd
    if ((mEventFd >= 0) && (close(mEventFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close event fd");

    // check no events were left in the queue
    for (Event &event : mEvents)
    {
        if ((event.pipeFd >= 0) && (close(event.pipeFd) != 0))
            AI_LOG_SYS_ERROR(errno, "failed to close pipe fd");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Creates a new logging client, which is just pipe with some meta data
 *  stored.
 *
 *
 *
 * @param[in]  id               The container id.
 * @param[in]  tag              The identifier to assign to all log messages.
 * @param[in]  allowedLevels    Bitmask of allowed levels.
 * @param[in]  rate             The rate limit (actually limiting TBD)
 * @param[in]  burstSize        The burst limit (actually limiting TBD)
 *
 * @return
 */
int EthanLogLoop::addClient(const ContainerId& id, const std::string &tag,
                            unsigned allowedLevels,
                            uint64_t rate, uint64_t burstSize)
{
    // sanity check the thread is running
    if (!mThread.joinable())
    {
        AI_LOG_ERROR("logging thread not running, can't create logging client '%s'",
                     tag.c_str());
        return -1;
    }

    // create the pipe in non-blocking mode and initially with the CLOEXEC
    // flag set (this will be cleared right before the container process is
    // forked safely)
    int fds[2];
    if (pipe2(fds, O_CLOEXEC | O_DIRECT | O_NONBLOCK) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create logging pipe");
        return -1;
    }

    // attempt to increase the pipe size limit to 256kB, this is quadruple
    // the default limit of 64kB
    const int loggingPipeSize = 256 * 1024;
    if (fcntl(fds[1], F_SETPIPE_SZ, loggingPipeSize) != 0)
    {
        AI_LOG_SYS_WARN(errno, "failed to set pipe size for logging pipe");
    }

    AI_LOG_DEBUG("create logging pipe : read=%d : write=%d", fds[0], fds[1]);

    // take the lock
    std::lock_guard<std::mutex> locker(mLock);

    // create a message for the event loop to pick up the new client
    // (the pipe fd is gifted to the event, it's responsible for closing it)
    mEvents.emplace_back(AddClientEvent(id, tag, fds[0], allowedLevels, rate, burstSize));

    // wake the event loop
    wakeLoop();

    // return the write end of the pipe
    return fds[1];
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the base pid number for the given container.
 *
 *  This is used so we can pass the real pid to journald.
 *
 * @param[in]  id           The container id.
 * @param[in]  basePid      The pid of the init process inside the container.
 */
void EthanLogLoop::setClientBasePid(const ContainerId& id, pid_t basePid)
{
    // sanity check the thread is running
    if (!mThread.joinable())
    {
        AI_LOG_ERROR("logging thread not running, can't set base pid for '%s'",
                     id.c_str());
        return;
    }

    // take the lock
    std::lock_guard<std::mutex> locker(mLock);

    // create a message for the event loop to set the base pid for the client
    mEvents.emplace_back(SetClientBasePidEvent(id, basePid));

    // wake the event loop
    wakeLoop();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Wakes the event loop.
 *
 *  Writes to the eventfd which should wake the event loop thread.  This is used
 *  when either a new client is added or we wish to terminate the event loop
 *  thread.
 *
 */
void EthanLogLoop::wakeLoop()
{
    if (mEventFd < 0)
    {
        AI_LOG_ERROR("no eventfd created");
    }

    uint64_t value = 1;
    if (TEMP_FAILURE_RETRY(write(mEventFd, &value, sizeof(value))) != sizeof(value))
    {
        AI_LOG_SYS_ERROR(errno, "failed to write to event fd");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Handler for wake ups from the event fd.
 *
 *  This will either be called if the terminate flag is set or when a new client
 *  has been added to the list of clients.
 *
 * @param[in]  source       The event loop source handler.
 * @param[in]  fd           Should be the event fd.
 * @param[in]  revents      The event that triggered the wake up.
 * @param[in]  userData     Pointer back to the instance of EthanLogLoop class.
 *
 * @return
 */
int EthanLogLoop::eventFdHandler(sd_event_source *source, int fd,
                                 uint32_t revents, void *userData)
{
    (void) revents;

    EthanLogLoop *self = reinterpret_cast<EthanLogLoop*>(userData);

    //
    if (fd != self->mEventFd)
    {
        AI_LOG_FATAL("invalid eventfd");
        return -1;
    }

    // read the eventfd to clear it
    uint64_t value;
    if (TEMP_FAILURE_RETRY(read(fd, &value, sizeof(value))) != sizeof(value))
    {
        AI_LOG_SYS_ERROR(errno, "failed to read from event fd");
    }

    // get the event loop
    sd_event *loop = sd_event_source_get_event(source);

    // take the lock
    std::lock_guard<std::mutex> locker(self->mLock);

    // first look if any clients need cleaning up
    auto it = self->mClients.begin();
    while (it != self->mClients.end())
    {
        std::unique_ptr<EthanLogClient> &client = *it;
        if (!client || client->closed())
            it = self->mClients.erase(it);
        else
            ++it;
    }

    // then process any events
    while (!self->mEvents.empty())
    {
        Event &event = self->mEvents.front();
        if (event.type == Event::Terminate)
        {
            sd_event_exit(loop, 0);
        }
        else if (event.type == Event::AddClient)
        {
            // create the new client wrapper
            std::unique_ptr<EthanLogClient> client =
                std::make_unique<EthanLogClient>(loop,
                                                 std::move(event.id),
                                                 std::move(event.tag),
                                                 event.pipeFd,
                                                 event.allowedLevels,
                                                 event.rate, event.burstSize,
                                                 self->mMemCgroupMountPoint);

            // if there was an error then the client is immediately closed,
            // check for that case and don't both adding
            if (!client->closed())
                self->mClients.emplace_back(std::move(client));
        }
        else if (event.type == Event::SetClientBasePid)
        {
            // find the client with the given container id
            for (const std::unique_ptr<EthanLogClient> &client : self->mClients)
            {
                if (client && (client->id() == event.id))
                    client->setContainerPid(event.basePid);
            }
        }
        else
        {
            AI_LOG_WARN("unknown event type %d", int(event.type));
        }

        // remove the event from the queue
        self->mEvents.pop_front();
    }

    return 0;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Event loop thread function.
 *
 *  Runs the event loop processing inputs from logging pipes until the terminate
 *  flag is set.
 *
 */
void EthanLogLoop::eventLoop()
{
    pthread_setname_np(pthread_self(), "DOBBY_ETHANLOG");

    sd_event *loop = nullptr;

    // create the loop
    int rc = sd_event_new(&loop);
    if ((rc < 0) || !loop)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to create sd-event loop");
        return;
    }

    // add an eventfd so we can wake the loop
    sd_event_source *eventSource = nullptr;
    rc = sd_event_add_io(loop, &eventSource, mEventFd, EPOLLIN,
                         &EthanLogLoop::eventFdHandler, this);
    if ((rc < 0) || !eventSource)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to add source for eventfd");

        sd_event_unref(loop);
        return;
    }

    // run the event loop until sd_event_exit is called
    sd_event_loop(loop);

    // clear all the clients
    {
        std::lock_guard<std::mutex> locker(mLock);
        mClients.clear();
    }

    // free the event loop
    sd_event_unref(loop);
}