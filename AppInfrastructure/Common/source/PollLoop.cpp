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
#include "PollLoop.h"
#include "Logging.h"

#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/syscall.h>
#if defined(__linux__)
#  include <sys/prctl.h>
#endif

#include <sys/eventfd.h>
#include <sys/timerfd.h>
#include <sys/epoll.h>

#include <thread>
#include <mutex>
#include <list>
#include <map>


#define MILLISEC_PER_NANOSEC   (1000L * 1000L)

using namespace AICommon;


// -----------------------------------------------------------------------------
/**
 * @brief Constructs a poll loop with the given name and restrictions
 *
 * A constructed poll loop is not automatically started, rather the caller should
 * call start to spawn a thread and run the poll loop inside it.
 *
 * @param[in]  name                 The name to give the poll loop thread
 *                                  (nb: thread names are limited to 16 characters).
 * @param[in]  maxSources           The maximum number of event sources that can
 *                                  be installed on the poll loop.
 * @param[in]  deferredTimeInterval The time interval in milliseconds of the
 *                                  deferred timer
 */
PollLoop::PollLoop(const std::string& name, int maxSources /*= 512*/,
                   long deferredTimeInterval /*= 20*/)
    : mName(name, 0, 15)
    , mEPollThreadId(-1)
    , mEPollFd(-1)
    , mDeathEventFd(-1)
    , mDeferTimerFd(-1)
    , mDeferTimerSpec({ .it_interval={ .tv_sec=0, .tv_nsec=(deferredTimeInterval * MILLISEC_PER_NANOSEC) },
                        .it_value=   { .tv_sec=0, .tv_nsec=(deferredTimeInterval * MILLISEC_PER_NANOSEC) } })
    , mMaxSources(maxSources + 2)
    , mDeferredSources(0)
{
}


// -----------------------------------------------------------------------------
/**
 * @brief Destructs the poll loop, tears down the thread if the loop is running
 *
 *
 *
 */
PollLoop::~PollLoop()
{
    stop();
}


// -----------------------------------------------------------------------------
/**
 * @brief Enables the deferred timer event source
 *
 * This is an internal function that is called when a source event has been
 * 'deferred', i.e. a client has called modSource(..., EPOLLDEFERRED).
 *
 *
 */
void PollLoop::enableDeferredTimer()
{
    // Should be called with the lock held, sanity check on debug builds
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mLock.try_lock() == true)
    {
        AI_LOG_ERROR("mutex lock not held in %s", __FUNCTION__);
        mLock.unlock();
    }
#endif

    // Enable the timer
    if (mDeferTimerFd >= 0)
    {
        if (timerfd_settime(mDeferTimerFd, 0, &mDeferTimerSpec, NULL) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to enable the defer timerfd");
        }
        else
        {
            AI_LOG_DEBUG("enabled deferred timerfd (it_interval:%0.03f, it_value:%0.03f)",
                         (mDeferTimerSpec.it_interval.tv_sec * 1000.0f) + (mDeferTimerSpec.it_interval.tv_nsec / 1000000000.0f),
                         (mDeferTimerSpec.it_value.tv_sec * 1000.0f) + (mDeferTimerSpec.it_value.tv_nsec / 1000000000.0f));
        }
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Disables the deferred timer event source
 *
 *
 *
 *
 */
void PollLoop::disableDeferredTimer()
{
    // Should be called with the lock held, sanity check on debug builds
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mLock.try_lock() == true)
    {
        AI_LOG_ERROR("mutex lock not held in %s", __FUNCTION__);
        mLock.unlock();
    }
#endif

    // Disable the timer
    if (mDeferTimerFd >= 0)
    {
        struct itimerspec timerSpec;
        bzero(&timerSpec, sizeof(timerSpec));

        if (timerfd_settime(mDeferTimerFd, 0, &timerSpec, NULL) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to disable the defer timerfd");
        }
        else
        {
            AI_LOG_DEBUG("disabled deferred timerfd");
        }
    }
}


// -----------------------------------------------------------------------------
/**
 * @brief Adds a new event source to the poll loop
 *
 * A source is a file descriptor, a bitmask of events to wait for and a IPollSource
 * object that will be called when any of the events in the bitmask occur on the
 * file descriptor.
 *
 * This method may fail if the number of sources installed exceeds the maximum
 * allowed.
 *
 * @param[in]  source     The source object to call process() on when an event
 *                        occurs
 * @param[in]  fd         The file descriptor to poll on
 * @param[in]  events     A bitmask of events to listen on
 *
 * @return true on success, false on failure.
 */
bool PollLoop::addSource(const std::shared_ptr<IPollSource>& source, int fd, uint32_t events)
{
    AI_LOG_FN_ENTRY();

    // Sanity check
    if ((fd < 0) || (fcntl(fd, F_GETFD) < 0))
    {
        AI_LOG_ERROR_EXIT("invalid file descriptor");
        return false;
    }

    std::lock_guard<Spinlock> locker(mLock);


    // Check we haven't exceeded the maximum number of event sources
    if (mSources.size() >= static_cast<size_t>(mMaxSources - 2))
    {
        AI_LOG_ERROR_EXIT("too many epoll sources");
        return false;
    }

    // Ensure only valid event flags are set
    events &= (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLDEFERRED);

    // Store the source and check if it has the deferred flag set
    mSources.push_back(PollSourceWrapper(source, fd, events));
    PollSourceWrapper & _source = mSources.back();

    if (_source.events & EPOLLDEFERRED)
    {
        if (++mDeferredSources == 1)
        {
            enableDeferredTimer();
        }
    }

    // Finally add it to epoll
    if (mEPollFd >= 0)
    {
        struct epoll_event event;
        bzero(&event, sizeof(event));
        event.data.fd = _source.fd;
        event.events = _source.events & (EPOLLIN | EPOLLOUT | EPOLLRDHUP);

        if (epoll_ctl(mEPollFd, EPOLL_CTL_ADD, fd, &event) < 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to add source to epoll");
            mSources.pop_back();
            return false;
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}


// -----------------------------------------------------------------------------
/**
 * @brief Modifies the events bitmask for the source
 *
 * This function can be used to change the events that a source is listening for.
 * The source must have successifully been added to the poll loop (@fn addSource())
 * prior to calling this method.
 *
 * @param[in]  source     The source object to modify the events for
 * @param[in]  events     The bitmask of the events to listen on now
 *
 * @return true on success, false on failure.
 */
bool PollLoop::modSource(const std::shared_ptr<IPollSource>& source, uint32_t events)
{
    // Ensure the event flags only have valid bits
    events &= (EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLDEFERRED);

    std::lock_guard<Spinlock> locker(mLock);

    // Try and find the source ... it means we have to temporarily lock all
    // the weak_ptrs to do the comparison
    std::list<PollSourceWrapper>::iterator it = mSources.begin();
    for (; it != mSources.end(); ++it)
    {
        if (it->source.lock() == source)
        {
            if (events != it->events)
            {
                // Check if the events to listen to have changed
                if ((it->events ^ events) & (EPOLLIN | EPOLLOUT | EPOLLRDHUP))
                {
                    if (mEPollFd >= 0)
                    {
                        struct epoll_event event;
                        bzero(&event, sizeof(event));
                        event.events = events & (EPOLLIN | EPOLLOUT | EPOLLRDHUP);
                        event.data.fd = it->fd;

                        if (epoll_ctl(mEPollFd, EPOLL_CTL_MOD, event.data.fd, &event) < 0)
                        {
                            AI_LOG_SYS_ERROR(errno, "failed to modify source to epoll");
                        }
                    }
                }

                // Check if the deferred flag is changed, in which case we enable or
                // disable the defer timer (if not already)
                if ((it->events ^ events) & EPOLLDEFERRED)
                {
                    mDeferredSources += (events & EPOLLDEFERRED) ? 1 : -1;

                    if (mDeferredSources == 1)
                    {
                        enableDeferredTimer();
                    }
                    else if (mDeferredSources == 0)
                    {
                        disableDeferredTimer();
                    }
                }

                it->events = events;
            }

            break;
        }
    }

#if (AI_BUILD_TYPE == AI_DEBUG)
    // Just for debugging ...
    if (it == mSources.end())
    {
        AI_LOG_ERROR("failed to find the source to modify");
    }
#endif

    return true;
}


// -----------------------------------------------------------------------------
/**
 * @brief Removes the source from the poll loop
 *
 * The source must have been previously added with @fn addSource.
 *
 * It's important to note that even after the source has been removed and this
 * function returns, it's possible for the source's process() method to be called.
 * This is because the poll loop thread locks the shared_ptrs while processing
 * the events.
 *
 * @param[in]  source   The source object to remove from the poll loop.
 *
 */
void PollLoop::delSource(const std::shared_ptr<IPollSource>& source)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<Spinlock> locker(mLock);

    // Try and find the source ... it means we have to temporarily lock all
    // the weak_ptrs to do the comparison
    std::list<PollSourceWrapper>::iterator it = mSources.begin();
    for (; it != mSources.end(); ++it)
    {
        if (it->source.lock() == source)
        {
            // decrement the list of deferred sources if set
            if (it->events & EPOLLDEFERRED)
            {
                if (--mDeferredSources == 0)
                {
                    disableDeferredTimer();
                }
            }

            // remove from epoll
            if (mEPollFd >= 0)
            {
                if (epoll_ctl(mEPollFd, EPOLL_CTL_DEL, it->fd, NULL) < 0)
                {
                    AI_LOG_SYS_ERROR_EXIT(errno, "failed to delete source from epoll");
                }
            }

            // erase from the list of sources and exit
            mSources.erase(it);

            AI_LOG_FN_EXIT();
            return;
        }
    }


    AI_LOG_ERROR_EXIT("failed to find the source to delete");
    return;
}


// -----------------------------------------------------------------------------
/**
 * @brief Starts the poll thread
 *
 * If the poll loop was already running it is stopped and restarted.
 *
 * @param[in]  priority    The SCRED_RR priority in which to run the poll loop,
 *                         if -1 the priority is inherited from the calling thread
 *
 * @return true on success, false on failure.
 */
bool PollLoop::start(int priority /* = -1 */)
{
    struct epoll_event event;

    AI_LOG_FN_ENTRY();

    // Call stop just in case we're already running
    stop();

    // Create an eventfd to signal death
    mDeathEventFd = eventfd(0, EFD_CLOEXEC | EFD_SEMAPHORE);
    if (mDeathEventFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create death eventfd");
        goto eventfd_failed;
    }

    // Create a timerfd for deferred processing of events
    mDeferTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (mDeferTimerFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create deferred timerfd");
        goto timerfd_failed;
    }

    // Create the epoll fd
    mEPollFd = epoll_create1(EPOLL_CLOEXEC);
    if (mEPollFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create epoll device");
        goto epollfd_failed;
    }


    // Add the eventfd to the epoll loop
    bzero(&event, sizeof(event));
    event.data.fd = mDeathEventFd;
    event.events = EPOLLIN;
    if (epoll_ctl(mEPollFd, EPOLL_CTL_ADD, mDeathEventFd, &event) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to add death eventfd to epoll");
        goto epolladd_failed;
    }

    // Add the timerfd to the epoll loop
    bzero(&event, sizeof(event));
    event.data.fd = mDeferTimerFd;
    event.events = EPOLLIN;
    if (epoll_ctl(mEPollFd, EPOLL_CTL_ADD, mDeferTimerFd, &event) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to add deferred timerfd to epoll");
        goto epolladd_failed;
    }



    // Add all the existing sources to the epoll loop (with spinlock held)
    {
        std::lock_guard<Spinlock> locker(mLock);

        if (!mSources.empty())
        {
            for (const PollSourceWrapper &wrapper : mSources)
            {
                // Check the source is valid before adding to epoll
                std::shared_ptr<IPollSource> _source = wrapper.source.lock();
                if (_source)
                {
                    bzero(&event, sizeof(event));
                    event.data.fd = wrapper.fd;
                    event.events = wrapper.events & (EPOLLIN | EPOLLOUT | EPOLLRDHUP);

                    if (epoll_ctl(mEPollFd, EPOLL_CTL_ADD, event.data.fd, &event) < 0)
                    {
                        AI_LOG_SYS_ERROR(errno, "failed to add source to epoll");
                    }
                }
            }

            // If any of the sources are deferred then start the timerfd now
            if (mDeferredSources > 0)
            {
                enableDeferredTimer();
            }
        }
    }


    // Finally spawn the thread that runs the poll loop
    mEPollThread = std::thread(&PollLoop::run, this, mName, priority);
    if (!mEPollThread.joinable())
    {
        AI_LOG_ERROR("failed to create jumper thread");
        goto epollfd_failed;
    }

    AI_LOG_FN_EXIT();
    return true;


epolladd_failed:
    close(mEPollFd);
    mEPollFd = -1;
epollfd_failed:
    close(mDeferTimerFd);
    mDeferTimerFd = -1;
timerfd_failed:
    close(mDeathEventFd);
    mDeathEventFd = -1;
eventfd_failed:

    AI_LOG_FN_EXIT();
    return false;
}

// -----------------------------------------------------------------------------
/**
 * @brief Stops the poll loop thread
 *
 * Stops the poll loop and cleans up all the resources associated with it.
 *
 *
 */
void PollLoop::stop()
{
    AI_LOG_FN_ENTRY();

    if (mDeathEventFd >= 0)
    {
        // Signal the eventfd which should cause the epoll thread to wake and
        // drop out
        uint64_t doesntMatter = 1;
        if (TEMP_FAILURE_RETRY(write(mDeathEventFd, &doesntMatter, sizeof(doesntMatter))) != sizeof(doesntMatter))
        {
            AI_LOG_SYS_ERROR(errno, "failed to signal death of epoll thread");
        }
        else if (mEPollThread.joinable())
        {
            // Wait for the thread to terminate, the thread will close all the
            // listening sockets
            mEPollThread.join();
        }

        close(mDeathEventFd);
        mDeathEventFd = -1;
    }

    if (mDeferTimerFd >= 0)
    {
        close(mDeferTimerFd);
        mDeferTimerFd = -1;
    }

    if (mEPollFd >= 0)
    {
        close(mEPollFd);
        mEPollFd = -1;
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 * @brief Returns the thread id of the poll loop thread.
 *
 * If the poll loop /threa is not current running, a default constructed
 * std::thread::id is returned.
 *
 */
std::thread::id PollLoop::threadId() const
{
    return mEPollThread.get_id();
}

// -----------------------------------------------------------------------------
/**
 * @brief Returns the linux thread id of the poll loop thread.
 *
 * If the poll loop /threa is not current running -1 will be returned.
 *
 */
pid_t PollLoop::gettid() const
{
    return mEPollThreadId.load();
}

// -----------------------------------------------------------------------------
/**
 * @brief The poll loop thread function
 *
 * This is the thread that does all the epoll stuff.
 *
 *
 */
void PollLoop::run(const std::string& name, int priority)
{
    AI_LOG_FN_ENTRY();

    // Store the thread id
    mEPollThreadId.store(syscall(SYS_gettid));

#if defined(__linux__)
    // As a general rule we block SIGPIPE - the most annoying signal in the world
    sigset_t sigpipeMask;
    sigemptyset(&sigpipeMask);
    sigaddset(&sigpipeMask, SIGPIPE);
    pthread_sigmask(SIG_BLOCK, &sigpipeMask, NULL);

    // Set the name of the thread
    if (!name.empty())
    {
        prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
    }
#endif

    // And (optionally) set the priority of the thread
    if (priority > 0)
    {
        struct sched_param param = { priority };
        pthread_setschedparam(pthread_self(), SCHED_RR, &param);
    }

    // Create event buffers to store all the input
    struct epoll_event *events = reinterpret_cast<struct epoll_event*>
        (calloc(mMaxSources, sizeof(struct epoll_event)));


    // Map of all the sources that we're triggered in one epoll cycle
    std::map<std::shared_ptr<IPollSource>, uint32_t> triggered;
    std::map<std::shared_ptr<IPollSource>, uint32_t>::iterator trigItor;

    std::list<PollSourceWrapper>::const_iterator srcItor;

    unsigned int failures = 0;
    bool done = false;
    while (!done)
    {
        // Wait for any epoll events
        int n = TEMP_FAILURE_RETRY(epoll_wait(mEPollFd, events, mMaxSources, -1));
        if (n < 0)
        {
            AI_LOG_SYS_ERROR(errno, "epoll_wait failed");

            if (++failures > 5)
            {
                AI_LOG_FATAL("too many errors occurred on epoll, shutting down loop");
                break;
            }
        }

        // Iterate through all the events
        for (int i = 0; i < n; i++)
        {
            const struct epoll_event *event = &events[i];

            // Check if requested to shutdown
            if (event->data.fd == mDeathEventFd)
            {
                done = true;
                break;
            }
            // Check if a deferred timer tick, in which case give each of the
            // deferred sources a chance to process some data
            else if (event->data.fd == mDeferTimerFd)
            {
                // Read the timerfd to clear the expire count and stop it
                // waking epoll until the next tick
                uint64_t expirations;
                if (TEMP_FAILURE_RETRY(read(mDeferTimerFd, &expirations, sizeof(expirations))) != sizeof(expirations))
                {
                    AI_LOG_SYS_ERROR(errno, "failed to read timerfd");
                }

                // AI_LOG_DEBUG("deferred timer tick (%llu expirations)", expirations);

                // Take the lock protecting access to mSources
                std::lock_guard<Spinlock> locker(mLock);

                // Add any deferred sources to the 'triggered' list
                for (srcItor = mSources.begin(); srcItor != mSources.end(); ++srcItor)
                {
                    if (srcItor->events & EPOLLDEFERRED)
                    {
                        std::shared_ptr<IPollSource> source = srcItor->source.lock();
                        if (source)
                        {
                            triggered[source] |= EPOLLDEFERRED;
                        }
                    }
                }
            }
            // Another event, iterate through the sources and compare their
            // fd's
            else
            {
                // Take the lock protecting access to mSources
                std::lock_guard<Spinlock> locker(mLock);

                for (srcItor = mSources.begin(); srcItor != mSources.end(); ++srcItor)
                {
                    if (event->data.fd == srcItor->fd)
                    {
                        // Perform another check to see if the events epoll gave
                        // us still match the ones in the PollSource set. These
                        // can get out of sync due to (valid) race conditions
                        // between epoll wake-up and taking the mLock
                        if (event->events & (srcItor->events | EPOLLRDHUP | EPOLLERR | EPOLLHUP))
                        {
                            std::shared_ptr<IPollSource> source = srcItor->source.lock();
                            if (source)
                            {
                                triggered[source] |= event->events;
                            }
                            else
                            {
                                // Failed to get the shared_ptr, should we remove
                                // it from the list of sources ?
                                AI_LOG_ERROR("failed to get source shared_ptr");
                            }
                        }
                    }
                }
            }
        }

        // mLock is no longer held which is ok as we now have a list of shared
        // pointers and their events, ensuring other threads can now add / delete
        // sources without affecting us


        // Iterate through the list of triggered sources and let them process
        // the events received
        for (trigItor = triggered.begin(); trigItor != triggered.end(); ++trigItor)
        {
            // Call process() with a reference back to ourselves and the events
            // triggered
            trigItor->first->process(shared_from_this(), trigItor->second);
        }

        // Clear the map of triggered sources
        triggered.clear();


        // And we're done, go back around and sleep
    }

    // Free the memory allocated for the events from epoll
    free(events);

    // Clear the thread id
    mEPollThreadId.store(-1);

    AI_LOG_FN_EXIT();
}


