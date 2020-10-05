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
#include "DbusEventDispatcher.h"
#include "DbusTimeouts.h"
#include "DbusWatches.h"

#include <Logging.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>

#include <poll.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>


using namespace AI_IPC;



DbusEventDispatcher::DbusEventDispatcher()
    : mDbusConnection(nullptr)
    , mDeathEventFd(-1)
    , mWakeupEventFd(-1)
    , mDispatchEventFd(-1)
    , mCallCounter(0)
{
    AI_LOG_FN_ENTRY();


    AI_LOG_FN_EXIT();
}


DbusEventDispatcher::~DbusEventDispatcher()
{
    AI_LOG_FN_ENTRY();

    // Check if the dispatch thread is still running, if so stop the event dispatcher
    if (mEventDispatcherThread.joinable())
    {
        stopEventDispatcher();
    }

    // Ensure all the events and their fds are closed
    cleanupAllEvents();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts the event dispatch thread for the given dbus connection
 *
 *  This creates the the eventfds for the wakeup, dispatch and death events.
 *  It then registers the libdbus callback functions before finally spawning the
 *  thread that runs the poll event loop.
 *
 *  @param[in]  connection      The dbus connection to run the event loop for.
 *
 */
void DbusEventDispatcher::startEventDispatcher(DBusConnection *connection)
{
    AI_LOG_FN_ENTRY();

    // Sanity check that the dispatch thread is not currently running
    if (mEventDispatcherThread.joinable())
    {
        AI_LOG_FATAL_EXIT("dispatch thread already running");
        return;
    }

    // Create the eventfds for the 'death', 'wakeup' and 'dispatch'
    mDeathEventFd = eventfd(0, EFD_CLOEXEC);
    if (mDeathEventFd < 0)
    {
        AI_LOG_SYS_FATAL_EXIT(errno, "failed to create eventfd for death");
        return;
    }

    mWakeupEventFd = eventfd(0, EFD_CLOEXEC);
    if (mWakeupEventFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create eventfd for wake-up");
        cleanupAllEvents();
        return;
    }

    mDispatchEventFd = eventfd(0, EFD_CLOEXEC);
    if (mDispatchEventFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create eventfd for dispatch");
        cleanupAllEvents();
        return;
    }

    mDbusConnection = connection;


    // Register function invoked when the dispatch status changes
    dbus_connection_set_dispatch_status_function(mDbusConnection,
                                                 DbusEventDispatcher::dispatchStatusCb,
                                                 this, NULL);

    // Register the function responsible for waking up the EventDispatcher thread
    dbus_connection_set_wakeup_main_function(mDbusConnection,
                                             DbusEventDispatcher::wakeUpCb,
                                             this, NULL);


    // And finally start the processing thread
    mEventDispatcherThread = std::thread(&DbusEventDispatcher::eventDispatcherThreadFn, this);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stops the event dispatch thread and cleans up all file descriptors
 *
 *  Starts by removing all the libdbus callbacks and then ask the thread to
 *  terminate by triggering the death eventfd.
 *
 */
void DbusEventDispatcher::stopEventDispatcher()
{
    AI_LOG_FN_ENTRY();

    if (!mEventDispatcherThread.joinable())
    {
        AI_LOG_ERROR_EXIT("dispatch thread not running");
        return;
    }

    // Un-register the dbus callbacks
    dbus_connection_set_dispatch_status_function(mDbusConnection, NULL, NULL, NULL);
    dbus_connection_set_wakeup_main_function(mDbusConnection, NULL, NULL, NULL);

    // Signal the death eventfd to kill the poll loop
    uint64_t killEvent = 1;
    if (TEMP_FAILURE_RETRY(write(mDeathEventFd, &killEvent, sizeof(killEvent))) != sizeof(killEvent))
    {
        AI_LOG_SYS_ERROR(errno, "failed to write to death eventfd");
    }

    // Join the thread
    mEventDispatcherThread.join();
    mDbusConnection = nullptr;

    AI_LOG_INFO( "Event dispatcher finished.." );

    // Clean-up the file descriptors
    cleanupAllEvents();

    // It's possible that some thread has queued up a callback which hasn't
    // been processed before the thread quit, we should wake up those threads
    // now to avoid them blocking forever
    std::lock_guard<std::mutex> locker(mCallLock);
    if (!mCallQueue.empty())
    {
        // Clear the queue without calling the callbacks
        while (!mCallQueue.empty())
            mCallQueue.pop();

        mCallCondVar.notify_all();
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Thread function that processes the events
 *
 *  The main poll thread that processes all dbus events.
 *
 *
 *
 */
void DbusEventDispatcher::eventDispatcherThreadFn()
{
    AI_LOG_FN_ENTRY();

    // Set the name of the thread
    pthread_setname_np(pthread_self(), "AI_DBUS_EVENTS");



    // Create the timeouts object to manage the dbus timeouts
    DbusTimeouts timeouts(mDbusConnection);
    int timeoutsFd = timeouts.fd();
    if (timeoutsFd < 0)
    {
        AI_LOG_FATAL("failed to attach timeout manager to the dbus connection");
    }

    // Create the watches object to manage the dbus watches
    DbusWatches watches(mDbusConnection);
    int watchesFd = watches.fd();
    if (watchesFd < 0)
    {
        AI_LOG_FATAL("failed to attach timeout manager to the dbus connection");
    }


    const nfds_t nPollFds = 5;
    struct pollfd pollFds[nPollFds];

    unsigned int failures = 0;
    bool done = false;
    while (!done)
    {
        AI_LOG_DEBUG("processing dispatch");

        // Run the dispatcher while data remains to be sent
        DBusDispatchStatus status = dbus_connection_get_dispatch_status(mDbusConnection);
        while (status == DBUS_DISPATCH_DATA_REMAINS)
        {
            dbus_connection_dispatch(mDbusConnection);
            status = dbus_connection_get_dispatch_status(mDbusConnection);
        }

        AI_LOG_DEBUG("processing callbacks");

        // Check if there is any functions to be called in this thread, we take
        // the lock, check the queue and then process each function.  We drop
        // the lock before calling the function to avoid any deadlocks in
        // client code
        std::unique_lock<std::mutex> locker(mCallLock);
        if (!mCallQueue.empty())
        {
            do
            {
                const std::function<void()>& fn = mCallQueue.front().second;

                locker.unlock();
                fn();
                locker.lock();

                mCallQueue.pop();

            } while (!mCallQueue.empty());

            mCallCondVar.notify_all();
        }
        locker.unlock();


        // Populate all the descriptors to poll on
        pollFds[0].fd = timeoutsFd;
        pollFds[0].events = POLLIN;
        pollFds[0].revents = 0;

        pollFds[1].fd = watchesFd;
        pollFds[1].events = POLLIN;
        pollFds[1].revents = 0;

        pollFds[2].fd = mDeathEventFd;
        pollFds[2].events = POLLIN;
        pollFds[2].revents = 0;

        pollFds[3].fd = mWakeupEventFd;
        pollFds[3].events = POLLIN;
        pollFds[3].revents = 0;

        pollFds[4].fd = mDispatchEventFd;
        pollFds[4].events = POLLIN;
        pollFds[4].revents = 0;


        AI_LOG_DEBUG("waiting on poll");

        // Wait for any poll events
        int nEvents = TEMP_FAILURE_RETRY(poll(pollFds, nPollFds, -1));
        if (nEvents < 0)
        {
            AI_LOG_SYS_ERROR(errno, "poll failed");

            if (++failures > 5)
            {
                AI_LOG_FATAL("too many errors occurred on poll, shutting down loop");
                break;
            }
            else
            {
                continue;
            }
        }

        // Process all the events
        for (nfds_t i = 0; i < nPollFds; i++)
        {
            if (pollFds[i].revents == 0)
                continue;

            AI_LOG_DEBUG("received [%s] event",
                         (pollFds[i].fd == mDeathEventFd)       ? "death" :
                         (pollFds[i].fd == mDispatchEventFd)    ? "dispatch" :
                         (pollFds[i].fd == mWakeupEventFd)      ? "wakeup" :
                         (pollFds[i].fd == watchesFd)           ? "watch" :
                         (pollFds[i].fd == timeoutsFd)          ? "timeout" : "??");


            // ---------------------------
            // Check if requested to shutdown
            if (pollFds[i].fd == mDeathEventFd)
            {
                done = true;
                break;
            }

            // ---------------------------
            // Check if a request to wakeup or dispatch status change
            else if ((pollFds[i].fd == mDispatchEventFd) ||
                     (pollFds[i].fd == mWakeupEventFd))
            {
                // Read the eventfd to clear it's state
                uint64_t ignore;
                if (TEMP_FAILURE_RETRY(read(pollFds[i].fd, &ignore,
                                            sizeof(ignore)) != sizeof(ignore)))
                {
                    AI_LOG_SYS_ERROR(errno, "failed to read dispatch eventfd");
                }

                // When dispatch status or Wakeup event is received
                // dbus_connection_dispatch should be called. Go to begin of while
                // loop which calls dbus_connection_dispatch.
            }

            // ---------------------------
            // Check if a dbus watch(es) has triggered
            else if (pollFds[i].fd == watchesFd)
            {
                watches.processEvent(pollFds[i].revents);
            }

            
            // ---------------------------
            // Check if a dbus timeout timer has expired
            else if (pollFds[i].fd == timeoutsFd)
            {
                timeouts.processEvent(pollFds[i].revents);
            }

        } // for (nfds_t i = 0; i < nPollFds; i++)

    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Closes the three eventfd's used to wake up and trigger events in
 *  the poll loop.
 *
 *  Upon return all eventfds will be closed.
 *
 */
void DbusEventDispatcher::cleanupAllEvents()
{
    AI_LOG_FN_ENTRY();

    // Remove the eventfds from epoll and then close
    const int eventFds[3] = { mDeathEventFd, mWakeupEventFd, mDispatchEventFd };
    for (unsigned int i = 0; i < 3; i++)
    {
        if (eventFds[i] >= 0)
        {
            if (TEMP_FAILURE_RETRY(close(eventFds[i])) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to close eventfd");
            }
        }
    }

    mDeathEventFd = -1;
    mWakeupEventFd = -1;
    mDispatchEventFd = -1;

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief libdbus callback when dispatch status changes.
 *
 *  This simply triggers the eventfd which wakes the poll loop and results
 *  in dbus_connection_dispatch() being called.
 *
 *  @param[in]  connection      The dbus connection object.
 *  @param[in]  status          The new status of the dbus loop.
 *  @param[in]  userData        Pointer to this.
 */
void DbusEventDispatcher::dispatchStatusCb(DBusConnection *connection,
                                           DBusDispatchStatus status,
                                           void *userData)
{
    AI_LOG_FN_ENTRY();

    DbusEventDispatcher* self = reinterpret_cast<DbusEventDispatcher*>(userData);

    if (status == DBUS_DISPATCH_DATA_REMAINS)
    {
        const uint64_t value = 1;

        if (self->mDispatchEventFd < 0)
        {
            AI_LOG_ERROR("no dispatch eventfd");
        }
        else if (TEMP_FAILURE_RETRY(write(self->mDispatchEventFd, &value, sizeof(value))) != sizeof(uint64_t))
        {
            AI_LOG_SYS_ERROR(errno, "failed to write to the dispatch event fd");
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief libdbus callback request to wake-up the event loop.
 *
 *  This simply triggers the eventfd which wakes the poll loop and results
 *  in dbus_connection_dispatch() being called.
 *
 *  @param[in]  userData        Pointer to this.
 */
void DbusEventDispatcher::wakeUpCb(void *userData)
{
    AI_LOG_FN_ENTRY();

    DbusEventDispatcher* self = reinterpret_cast<DbusEventDispatcher*>(userData);

    const uint64_t value = 1;

    if (self->mWakeupEventFd < 0)
    {
        AI_LOG_ERROR("no wakeup eventfd");
    }
    else if (TEMP_FAILURE_RETRY(write(self->mWakeupEventFd, &value, sizeof(value))) != sizeof(uint64_t))
    {
        AI_LOG_SYS_ERROR(errno, "failed to write to the wakeup event fd");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Calls the supplied function in the context of the dispatch thread
 *
 *  @warning Because the method is called from the context of the dispatch
 *  thread, avoid calling from an dbus callback with locks held otherwise
 *  you could end up deadlocked.
 *
 *  It is safe to call this method from inside or outside the dispatch thread;
 *  if called from within, the function is directly executed, if not in the
 *  dispatcher thread then the function is queued up and the method will
 *  block until the function has been processed within the thread.
 *
 *  @param[in]  func    The function to run in the dispatcher thread
 *
 *  @return true if the function was executed, otherwise false.
 */
bool DbusEventDispatcher::callInEventLoopImpl(const std::function<void()>& func)
{
    AI_LOG_FN_ENTRY();

    // if the dispatcher thread isn't running ... then we're in trouble
    if (!mEventDispatcherThread.joinable())
    {
        AI_LOG_ERROR_EXIT("dispatcher thread not running");
        return false;
    }

    // if we're already in the dispatcher thread then just execute the function
    if (std::this_thread::get_id() == mEventDispatcherThread.get_id())
    {
        func();

        AI_LOG_FN_EXIT();
        return true;
    }

    // sanity check we have a wake-up eventfd
    if (mWakeupEventFd < 0)
    {
        AI_LOG_ERROR_EXIT("no wakeup eventfd");
        return false;
    }

    // otherwise add the function to the queue
    std::unique_lock<std::mutex> locker(mCallLock);

    // try and wake the event loop, we can do this before pushing the value
    // in the queue as we currently hold the lock ... and this means we don't
    // push the function if for whatever reason we can write to the wake
    // eventfd
    uint64_t value = 1;
    if (TEMP_FAILURE_RETRY(write(mWakeupEventFd, &value, sizeof(value))) != sizeof(uint64_t))
    {
        AI_LOG_SYS_ERROR(errno, "failed to write to the wakeup event fd");
        return false;
    }

    // the tag number is used to determine if the call has completed, we look
    // at the head of the queue when woken and if the tag number is higher then
    // we must have executed the function
    const uint64_t callTag = mCallCounter++;
    mCallQueue.emplace(callTag, func);


    while (true)
    {
        mCallCondVar.wait(locker);

        // if the thread is no longer running then we've failed
        if (!mEventDispatcherThread.joinable())
        {
            AI_LOG_ERROR_EXIT("dispatcher thread stopped while waiting to execute func");
            return false;
        }

        // if the queue is now empty or the head of the queue has a higher
        // tag number then the function has been executed
        if (mCallQueue.empty() || (mCallQueue.front().first > callTag))
        {
            AI_LOG_FN_EXIT();
            return true;
        }
    }

    // no way out this way
}


