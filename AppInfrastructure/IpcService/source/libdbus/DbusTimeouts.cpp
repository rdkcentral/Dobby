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
 *  DbusTimeouts.cpp
 *
 */

#include "DbusTimeouts.h"

#include <Logging.h>

#include <climits>

#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/timerfd.h>


using namespace AI_IPC;


DbusTimeouts::DbusTimeouts(DBusConnection *conn)
    : mTimerFd(-1)
    , mDbusConnection(conn)
    , mWithinEventHandler(false)
#if (AI_BUILD_TYPE == AI_DEBUG)
    , mExpectedThreadId(std::this_thread::get_id())
#endif
{
    AI_LOG_FN_ENTRY();

    // create a timerfd for adding to the poll loop and using for timeout
    // wake-ups
    mTimerFd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
    if (mTimerFd < 0)
    {
        AI_LOG_SYS_FATAL_EXIT(errno, "failed to create timerfd");
        return;
    }

    // set the timeout functions, these functions are responsible for making the
    // even dispatcher thread aware of timeouts
    dbus_bool_t status = dbus_connection_set_timeout_functions(conn,
                                                               DbusTimeouts::addTimeOutCb,
                                                               DbusTimeouts::removeTimeOutCb,
                                                               DbusTimeouts::toggleTimeOutCb,
                                                               this, NULL);
    if (status != TRUE)
    {
        AI_LOG_ERROR_EXIT("dbus_connection_set_timeout_functions failed");
        return;
    }

    AI_LOG_FN_EXIT();
}

DbusTimeouts::~DbusTimeouts()
{
    AI_LOG_FN_ENTRY();

#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // clear all the callback functions
    dbus_connection_set_timeout_functions(mDbusConnection, NULL, NULL, NULL, NULL, NULL);

    // close the timerfd
    if ((mTimerFd >= 0) && (TEMP_FAILURE_RETRY(close(mTimerFd)) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close timerfd");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the timerfd that the dispatcher should poll on
 *
 *
 */
int DbusTimeouts::fd() const
{
    return mTimerFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called when something has happened on the timerfd event loop
 *
 *  The main disaptcher loop polls on the timerfd we supply, when anything
 *  changes then this method is called.
 *
 *  @param[in]  pollEvents  Bitmask of the poll events that woke the main loop.
 *
 */
void DbusTimeouts::processEvent(unsigned int pollEvents)
{
    AI_LOG_FN_ENTRY();

#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // not really sure what we can do if an error is received, for now just
    // log it
    if (pollEvents & (POLLERR | POLLHUP))
    {
        AI_LOG_ERROR("unexpected error / hang-up detected on timerfd");
    }

    // read the timerfd to clear the value
    uint64_t ignore;
    if (read(mTimerFd, &ignore, sizeof(uint64_t)) != sizeof(uint64_t))
    {
        if (errno != EAGAIN)
            AI_LOG_SYS_ERROR(errno, "failed to read from timerfd");
    }

    // get the current monotonic time and check if anyone has expired
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);


    // loop through the timeouts, any that have now expired get 'handled',
    // however note that the dbus_timeout_handle may call any of our callbacks
    // so the list entries may be modified while they are processed
    mWithinEventHandler = true;

    for (TimeoutEntry& entry : mTimeouts)
    {
        if (entry.timeout == nullptr)
            continue;
        if (hasExpired(entry.expiry, now) == false)
            continue;

        if (dbus_timeout_get_enabled(entry.timeout) == TRUE)
        {
            // reset the timeout for the next interval, this may be overridden
            // by the dbus code called by the following handler
            entry.expiry = calcAbsTime(now, dbus_timeout_get_interval(entry.timeout));

            // handle the timeout
            dbus_timeout_handle(entry.timeout);
        }
    }

    mWithinEventHandler = false;


    // now we've called any timeouts that have expired, try and clean up the
    // list plus re-sort it so the next item to expire is at the head
    std::list<TimeoutEntry>::iterator it = mTimeouts.begin();
    while (it != mTimeouts.end())
    {
        if ((it->timeout == nullptr) || (dbus_timeout_get_enabled(it->timeout) == FALSE))
        {
            it = mTimeouts.erase(it);
        }
        else
        {
            it = std::next(it);
        }
    }

    mTimeouts.sort();


    // finally update the timerfd for the next item that is on the head of the
    // timer queue (if any)
    updateTimerFd();


    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calculates a new time value based on the time now and the supplied
 *  millisecond offset.
 *
 *  @param[in]  base        The base time to calculate the new offset from
 *  @param[in]  offset      The milliseconds offset
 *
 *  @return a timespec that is the base value plus the offset.
 */
struct timespec DbusTimeouts::calcAbsTime(const struct timespec& base,
                                          int milliseconds) const
{
    #define NSECS_PER_SEC   1000000000L
    #define NSECS_PER_MSEC  1000000L

    struct timespec ts;

    if (milliseconds <= 0)
    {
        AI_LOG_WARN("timeout milliseconds is <= 0");

        // just return the base address, don't return a time way in the past
        ts = base;
    }
    else
    {
        ts.tv_sec = base.tv_sec + (milliseconds / 1000u);
        ts.tv_nsec = base.tv_nsec + ((milliseconds % 1000u) * NSECS_PER_MSEC);

        if (ts.tv_nsec > NSECS_PER_SEC)
        {
            ts.tv_nsec -= NSECS_PER_SEC;
            ts.tv_sec += 1;
        }
    }

    return ts;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility function that simply checks if a timespec is after or equal
 *  to another timespec.
 *
 *  This is used to determine if a timer has expired or not.
 *
 *  @param[in]  expiryTime      The expiry time point.
 *  @param[in]  currentTime     The current time point.
 *
 *  @return true if expiryTime is less than or equal to the currentTime.
 */
bool DbusTimeouts::hasExpired(const struct timespec& expiryTime,
                              const struct timespec& currentTime) const
{
    return (expiryTime.tv_sec < currentTime.tv_sec) ||
           ((expiryTime.tv_sec == currentTime.tv_sec) &&
            (expiryTime.tv_nsec <= currentTime.tv_nsec));
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes the item on the head of the expiry queue into the timerfd
 *  for the next wake-up time
 *
 *  If the expiry queue is empty then 0 is written into the timerfd which
 *  disables it.
 *
 */
void DbusTimeouts::updateTimerFd() const
{
    struct itimerspec its;
    its.it_interval.tv_sec = 0;
    its.it_interval.tv_nsec = 0;

    // by default disable the timerfd
    its.it_value.tv_sec = 0;
    its.it_value.tv_nsec = 0;

    // check we have any timeouts
    if (!mTimeouts.empty())
    {
        // if so use the head of the queue for the timeout
        its.it_value = mTimeouts.front().expiry;
    }

    // finally update the timerfd to either; disable it or set to the next
    // expiry value
    if (timerfd_settime(mTimerFd, TFD_TIMER_ABSTIME, &its, NULL) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to set timerfd value");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds the given timeout to the timerfd to poll on
 *
 *  @param[in]  timeout     The timeout object to add
 *
 *  @return TRUE if the timeout was added, otherwise FALSE.
 */
dbus_bool_t DbusTimeouts::addTimeOut(DBusTimeout *timeout)
{
    AI_LOG_FN_ENTRY();

    // debugging check to ensure we're being called from the correct thread
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // sanity check the timeout is enabled, if not ignore
    if (!dbus_timeout_get_enabled(timeout))
    {
        AI_LOG_ERROR_EXIT("libdbus trying to add disabled timeout");
        return FALSE;
    }

    // get the timeout interval
    const int interval = dbus_timeout_get_interval(timeout);
    if (interval <= 0)
    {
        AI_LOG_ERROR_EXIT("libdbus trying to add timeout with invalid "
                          "interval (%d)", interval);
        return FALSE;
    }

    // wrap the timeout in an entry thingy
    TimeoutEntry entry;
    entry.timeout = timeout;

    // calculate the expiry time of the timeout
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    entry.expiry = calcAbsTime(now, interval);

    // apparently we can add a new entry to a list without invalidating
    // iterators or references, hence we can append this entry without caring
    // if already processing timeouts
    mTimeouts.emplace_front(std::move(entry));

    // if not currently processing the timeouts then we may need to update
    // the timerfd as this element might now be the next item to expire
    if (!mWithinEventHandler)
    {
        // reset the list so the next to expire as at the head
        mTimeouts.sort();

        // update the timerfd if the new head of the list is the one we just
        // added
        if (mTimeouts.front().timeout == timeout)
            updateTimerFd();
    }
    
    AI_LOG_FN_EXIT();
    return TRUE;
}

dbus_bool_t DbusTimeouts::addTimeOutCb(DBusTimeout *timeout, void *userData)
{
    DbusTimeouts* self = reinterpret_cast<DbusTimeouts*>(userData);
    return self->addTimeOut(timeout);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the timeout from the timerfd
 *
 *
 *
 *  @param[in]  timeout     The timeout object to remove
 */
void DbusTimeouts::removeTimeOut(DBusTimeout *timeout)
{
    AI_LOG_FN_ENTRY();

    // debugging check to ensure we're being called from the correct thread
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // find the timeout in our list
    std::list<TimeoutEntry>::iterator it = mTimeouts.begin();
    for (; it != mTimeouts.end(); ++it)
    {
        if (it->timeout == timeout)
            break;
    }

    if (it == mTimeouts.end())
    {
        AI_LOG_ERROR_EXIT("failed to find timeout to remove");
        return;
    }

    // if we're in the middle of processing the timeout array, don't remove an
    // element, instead we just set the timeout pointer to null, the processing
    // loop will clean up
    if (mWithinEventHandler)
    {
        it->timeout = nullptr;
        it->expiry.tv_sec = INT_MAX;
    }
    else
    {
        // if not processing then we can just remove the entry, in theory we
        // should also update the timerfd, but we only need to do this if we're
        // removing the next expiry ... and it's probably just more efficent to
        // let the timer expire and handle it in processEvent()
        mTimeouts.erase(it);
    }
}

void DbusTimeouts::removeTimeOutCb(DBusTimeout *timeout, void *userData)
{
    DbusTimeouts* self = reinterpret_cast<DbusTimeouts*>(userData);
    self->removeTimeOut(timeout);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Toggles the enable / disable state of a timeout
 *
 *
 *  @param[in]  timeout     The timeout object to toggle
 */
void DbusTimeouts::toggleTimeOut(DBusTimeout *timeout)
{
    AI_LOG_FN_ENTRY();

    // debugging check to ensure we're being called from the correct thread
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // if in the middle of processing the timeouts then we don't need to do
    // anything as the code checks if the timeout is enabled at the end of
    // the processing loop
    if (mWithinEventHandler)
    {
        AI_LOG_FN_EXIT();
        return;
    }

    // find the element in the queue
    std::list<TimeoutEntry>::iterator it = mTimeouts.begin();
    for (; it != mTimeouts.end(); ++it)
    {
        if (it->timeout == timeout)
            break;
    }

    if (it == mTimeouts.end())
    {
        AI_LOG_ERROR_EXIT("failed to find timeout to toggle");
        return;
    }


    // get the time interval
    const int interval = dbus_timeout_get_interval(timeout);

    // if the timeout is (now) enabled, then reset the expiry
    if (dbus_timeout_get_enabled(timeout) && (interval > 0))
    {
        // calculate the new expiry time
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        it->expiry = calcAbsTime(now, interval);

        // re-sort the list as the order of timeouts may have changed
        mTimeouts.sort();
    }
    else
    {
        mTimeouts.erase(it);
    }

    // we may need to update the timerfd as we could have toggled the first
    // expiry entry in the queue
    updateTimerFd();

    AI_LOG_FN_EXIT();
}

void DbusTimeouts::toggleTimeOutCb(DBusTimeout *timeout, void *userData)
{
    DbusTimeouts* self = reinterpret_cast<DbusTimeouts*>(userData);
    self->toggleTimeOut(timeout);
}


