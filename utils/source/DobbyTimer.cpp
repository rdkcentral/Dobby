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
 * File:   DobbyTimer.cpp
 *
 */
#include "DobbyTimer.h"

#include <Logging.h>

#include <list>

#include <poll.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/eventfd.h>


DobbyTimer::DobbyTimer()
    : mTimerFd(-1)
    , mEventFd(-1)
{
    AI_LOG_FN_ENTRY();

    mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
    if (mTimerFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create timerfd");
        return;
    }

    mEventFd = eventfd(0, EFD_CLOEXEC);
    if (mEventFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create eventFd");
        return;
    }

    mThread = std::thread(&DobbyTimer::timerThread, this);
    
    AI_LOG_FN_EXIT();
}

DobbyTimer::~DobbyTimer()
{
    AI_LOG_FN_ENTRY();

    // ensure the thread is stopped
    stop();

    // clean up the timer and event fds
    if ((mTimerFd >= 0) && (close(mTimerFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close timerfd");
    if ((mEventFd >= 0) && (close(mEventFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close eventfd");

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stops the poll loop thread and cancels all timers
 *
 *  
 *
 */
void DobbyTimer::stop()
{
    AI_LOG_FN_ENTRY();

    // if the thread is still running, terminate by triggering the eventfd
    if (mThread.joinable())
    {
        // write to the eventfd to wake the poll loop
        uint64_t doesntMatter = 1;
        if (TEMP_FAILURE_RETRY(write(mEventFd, &doesntMatter, sizeof(uint64_t))) != sizeof(uint64_t))
        {
            AI_LOG_SYS_ERROR(errno, "failed to write to eventfd");
        }

        // wait for the thread to die
        mThread.join();
    }

    mTimersQueue.clear();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calculates the a new time value based on the time now and the
 *  supplied millisecond offset.
 *
 *  @param[in]  base        The base time to calculate the new offset from
 *  @param[in]  offset      The milliseconds offset
 *
 *  @return a timespec that is the base value plus the offset.
 */
struct timespec DobbyTimer::calcAbsTime(const struct timespec& base,
                                        const std::chrono::milliseconds& offset) const
{
    #define NSECS_PER_SEC   1000000000L
    #define NSECS_PER_MSEC  1000000L

    struct timespec ts;

    ts.tv_sec = base.tv_sec + std::chrono::duration_cast<std::chrono::seconds>(offset).count();
    ts.tv_nsec = base.tv_nsec + ((offset % std::chrono::seconds(1)).count() * NSECS_PER_MSEC);

    if (ts.tv_nsec > NSECS_PER_SEC)
    {
        ts.tv_nsec -= NSECS_PER_SEC;
        ts.tv_sec += 1;
    }

    return ts;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a new timer to the timer queue
 *
 *  
 *  @param[in]  timeout     The time after which to call the supplied handler.
 *  @param[in]  oneShot     If true the timer is automatically removed after
 *                          it times out the first time.
 *  @param[in]  handler     The handler function to call when the timer times
 *                          out.
 *
 *  @return on success returns a (greater than zero) timer id integer which
 *  identifies the timer, on failure -1 is returned.
 */
int DobbyTimer::add(const std::chrono::milliseconds& timeout, bool oneShot,
                    const std::function<bool()>& handler)
{
    AI_LOG_FN_ENTRY();

    // check the timer thread is running
    if (!mThread.joinable())
    {
        AI_LOG_ERROR_EXIT("timer thread not running");
        return -1;
    }

    // get a new id, will fail if exhausted
    int id = mIdGenerator.get();
    if (id < 0)
    {
        AI_LOG_ERROR_EXIT("exhausted timer id pool");
        return -1;
    }

    // convert the time point to a *nix style timespec
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    // create the entry
    TimerEntry entry;
    entry.id = id;
    entry.oneshot = oneShot;
    entry.func = handler;
    entry.timeout = timeout;
    entry.expiry = calcAbsTime(now, timeout);

    // take the lock and push the timer into the prioity queue
    std::lock_guard<std::recursive_mutex> locker(mLock);

    std::multiset<TimerEntry, TimerEntryCompare>::iterator it = mTimersQueue.emplace(entry);

    // if the new timer was added to the head of the queue then update the
    // timerfd
    if (it == mTimersQueue.begin())
        updateTimerFd();

    AI_LOG_FN_EXIT();
    return id;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the given timer from the timer queue
 *
 *  Once this method returns  (successfully) you are guaranteed that the timer
 *  handler will not be called, i.e. this is synchronisation point.
 *
 *  This method will fail if called from the context of a timer handler, if
 *  you want to cancel a repeating timer then just return false in the handler.
 *
 *  @param[in]  timerId     The id of the timer to remove as returned by the
 *                          add() method
 *
 *  @return true if the timer was found and was removed from the queue,
 *  otherwise false
 */
bool DobbyTimer::remove(int timerId)
{
    AI_LOG_FN_ENTRY();

    // take the lock and try and find the timerid in the queue
    std::lock_guard<std::recursive_mutex> locker(mLock);

    // check the timer thread is running and we're not be called recursively
    if (!mThread.joinable())
    {
        AI_LOG_ERROR_EXIT("timer thread not running");
        return false;
    }
    if (std::this_thread::get_id() == mThread.get_id())
    {
        AI_LOG_ERROR_EXIT("not allowed to call remove from a timer handler, "
                          "instead return false from the handler");
        return false;
    }

    // find the timer in the queue
    std::multiset<TimerEntry, TimerEntryCompare>::iterator it = mTimersQueue.begin();
    for (; it != mTimersQueue.end(); ++it)
    {
        if (it->id == timerId)
        {
            // if we removing the item from the head of the queue then need to
            // update the expiry time in the timerfd
            bool requiresUpdate = (it == mTimersQueue.begin());

            // remove the timer from the queue
            mTimersQueue.erase(it);

            // put the timerid back in the pool
            mIdGenerator.put(timerId);

            // update the timerfd if required
            if (requiresUpdate)
                updateTimerFd();

            AI_LOG_FN_EXIT();
            return true;
        }
    }

    AI_LOG_ERROR_EXIT("failed to find timer with id %d to remove", timerId);
    return false;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes the item on the head of the expiry queue into the timerfd
 *  for the next wake-up time
 *
 *
 *
 */
void DobbyTimer::updateTimerFd() const
{
    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    if (mTimersQueue.empty())
    {
        // this will disable the timerfd
        its.it_value.tv_sec = 0;
        its.it_value.tv_nsec = 0;
    }
    else
    {
        // set it's expiry value
        its.it_value = mTimersQueue.begin()->expiry;
    }

    if (timerfd_settime(mTimerFd, TFD_TIMER_ABSTIME, &its, NULL) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to set timerfd value");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief The thread function that runs the timer poll loop
 *
 *  This simply polls on an timerfd and eventfd.  The timerfd is obviously for
 *  waking up and calling any installed timers at the right time.  The eventfd
 *  is used to kill the poll loop at shutdown time.
 *
 *
 */
void DobbyTimer::timerThread()
{
    AI_LOG_FN_ENTRY();

    pthread_setname_np(pthread_self(), "AI_DOBBY_TIMER");

    if (mTimerFd < 0)
    {
        AI_LOG_ERROR_EXIT("no timerfd available");
        return;
    }
    if (mEventFd < 0)
    {
        AI_LOG_ERROR_EXIT("no eventfd available");
        return;
    }

    while (true)
    {
        struct pollfd fds[2];

        fds[0].fd = mEventFd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;

        fds[1].fd = mTimerFd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int ret = TEMP_FAILURE_RETRY(poll(fds, 2, -1));
        if (ret < 0)
        {
            AI_LOG_SYS_ERROR(errno, "poll failed");
        }
        else if (ret > 0)
        {
            // check if the eventfd has fired
            if (fds[0].revents != 0)
            {
                // check for any error conditions
                if (fds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
                {
                    AI_LOG_ERROR("received error events on eventfd (0x%04x)",
                                 fds[0].revents);
                }

                // read the eventfd to clear the value
                uint64_t ignore;
                if (TEMP_FAILURE_RETRY(read(mEventFd, &ignore, sizeof(uint64_t))) != sizeof(uint64_t))
                {
                    AI_LOG_SYS_ERROR(errno, "failed to read from eventfd");
                }

                // break out of the poll loop
                break;
            }

            // check if the timerfd fired
            if (fds[1].revents != 0)
            {
                // check for any error conditions
                if (fds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
                {
                    AI_LOG_ERROR("received error events on timerfd (0x%04x)",
                                 fds[1].revents);
                }

                // read the timerfd to clear the value
                uint64_t ignore;
                if (TEMP_FAILURE_RETRY(read(mTimerFd, &ignore, sizeof(uint64_t))) != sizeof(uint64_t))
                {
                    AI_LOG_SYS_ERROR(errno, "failed to read from timerfd");
                }


                // get the current monotonic time and check if anyone has expired
                struct timespec now;
                clock_gettime(CLOCK_MONOTONIC, &now);


                // take the lock and then move all the expired timers into a
                // separate set for processing, we then optionally put any back
                // that want to be rescheduled
                std::lock_guard<std::recursive_mutex> locker(mLock);

                std::list<TimerEntry> expired;
                std::multiset<TimerEntry, TimerEntryCompare>::iterator it = mTimersQueue.begin();
                while ((it != mTimersQueue.end()) && (it->isLessThanOrEqualTo(now)))
                {
                    // put in the expired set and remove from the master set
                    expired.emplace_back(std::move(*it));
                    it = mTimersQueue.erase(it);
                }

                // got through the expired timers and call their handlers, we
                // then reschedule the non-oneshot timers
                for (TimerEntry entry : expired)
                {
                    // call the callback function, if it returns true and it's
                    // not a one shot timer then reschedule
                    if (entry.func && entry.func() && !entry.oneshot)
                    {
                        entry.expiry = calcAbsTime(now, entry.timeout);
                        mTimersQueue.emplace(std::move(entry));
                    }
                    else
                    {
                        // release the timer id
                        mIdGenerator.put(entry.id);
                    }
                }

                // finally update the timerfd for the next item that is on the
                // head of the timer queue (if any)
                updateTimerFd();
            }
        }

    }

    AI_LOG_FN_EXIT();
}

