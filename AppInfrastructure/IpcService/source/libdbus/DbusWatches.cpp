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
 *  DbusWatches.cpp
 *
 */

#include "DbusWatches.h"

#include <Logging.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <cinttypes>


using namespace AI_IPC;


DbusWatches::DbusWatches(DBusConnection *conn)
    : mDbusConnection(conn)
    , mEpollFd(-1)
    , mTagCounter(0)
#if (AI_BUILD_TYPE == AI_DEBUG)
    , mExpectedThreadId(std::this_thread::get_id())
#endif
{
    AI_LOG_FN_ENTRY();

    bzero(&mWatches, sizeof(mWatches));

    for (unsigned n = 0; n < mMaxWatches; n++)
        mWatches[n].fd = -1;


    // create another epoll fd that is used for all the watches
    mEpollFd = epoll_create1(EPOLL_CLOEXEC);
    if (mEpollFd < 0)
    {
        AI_LOG_SYS_FATAL_EXIT(errno, "failed to create epoll fd");
        return;
    }


    // set the watch function, these functions are responsible for making the
    // even dispatcher thread aware of file descriptors that need to be
    // monitored for events.
    dbus_bool_t status = dbus_connection_set_watch_functions(conn,
                                                             DbusWatches::addWatchCb,
                                                             DbusWatches::removeWatchCb,
                                                             DbusWatches::toggleWatchCb,
                                                             this, NULL);
    if (status != TRUE)
    {
        AI_LOG_ERROR_EXIT("dbus_connection_set_watch_functions failed");
        return;
    }


    AI_LOG_FN_EXIT();
}

DbusWatches::~DbusWatches()
{
    AI_LOG_FN_ENTRY();

#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // clear all the callback functions
    dbus_connection_set_watch_functions(mDbusConnection, NULL, NULL, NULL, NULL, NULL);

    // close the epoll descriptor
    if ((mEpollFd >= 0) && (TEMP_FAILURE_RETRY(close(mEpollFd)) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close epoll fd");
    }

    // note that it's likely that watches may be left in the mWatches array
    // at this point, this is ok however we should close their dup'd file
    // descriptors as we no longer need them
    for (unsigned n = 0; n < mMaxWatches; n++)
    {
        if (mWatches[n].fd >= 0)
        {
            if (TEMP_FAILURE_RETRY(close(mWatches[n].fd)) != 0)
            {
                AI_LOG_SYS_ERROR(errno, "failed to close dup'd fd");
            }

            mWatches[n].fd = -1;
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the epoll fd that the dispatcher should poll on
 *
 *
 */
int DbusWatches::fd() const
{
    return mEpollFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called when something has happened on the epoll event loop
 *
 *  The main disaptcher loop polls on the fd we supply, when anything changes
 *  then this method is called.
 *
 *  @param[in]  pollEvents  Bitmask of the poll events that woke the main loop.
 *
 */
void DbusWatches::processEvent(unsigned pollEvents)
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
        AI_LOG_ERROR("unexpected error / hang-up detected on epoll fd");
    }

    // get any epoll events
    int nEvents = TEMP_FAILURE_RETRY(epoll_wait(mEpollFd, mEpollEvents, mMaxWatches, 0));
    if (nEvents < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "epoll_wait failed");
        return;
    }

    // process all the events
    for (int i = 0; i < nEvents; i++)
    {
        const uint32_t events = mEpollEvents[i].events;
        const uint64_t tag = mEpollEvents[i].data.u64;
        const size_t idx = static_cast<size_t>(tag % mMaxWatches);

        if (mWatches[idx].tag != tag)
        {
            AI_LOG_ERROR("invalid tag value (tag=%" PRIu64 ")", tag);
            return;
        }
        else if (mWatches[idx].watch == nullptr)
        {
            AI_LOG_ERROR("trying to handle a watch that doesn't exist (tag=%" PRIu64 ")", tag);
            return;
        }

        if (dbus_watch_get_enabled(mWatches[idx].watch))
        {
            // convert the epoll event flags to dbus watch flags
            unsigned int dbusWatchFlags = 0;
            if (events & EPOLLIN)
                dbusWatchFlags |= DBUS_WATCH_READABLE;
            if (events & EPOLLOUT)
                dbusWatchFlags |= DBUS_WATCH_WRITABLE;
            if (events & EPOLLERR)
                dbusWatchFlags |= DBUS_WATCH_ERROR;
            if (events & EPOLLHUP)
                dbusWatchFlags |= DBUS_WATCH_HANGUP;

            dbus_watch_handle(mWatches[idx].watch, dbusWatchFlags);
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Tries to find a slot to put the watch into
 *
 *  We store the watches in an internal array, each watch has a unique 64-bit
 *  tag number, this tag is added to the epoll event so we can quickly find
 *  the watch that triggered the event.
 *
 *  The lower bits of the tag number are the index into the internal array.
 *
 *  @param[in]  watch       The dbus watch object
 *  @param[in]  duppedFd    The dup'ed file descriptor for the watch
 *
 *  @return on success a non-zero tag number, on failure 0.
 */
uint64_t DbusWatches::createWatch(DBusWatch *watch, int duppedFd)
{
    // find the next available tag, the lower bits of the tag are used for
    // the index into the slots array, so we just need to find an empty slot
    for (unsigned n = 0; n < mMaxWatches; n++)
    {
        const size_t idx = static_cast<size_t>(++mTagCounter % mMaxWatches);
        if (mWatches[idx].watch == nullptr)
        {
            // set this slot as 'in use'
            mWatches[idx].fd = duppedFd;
            mWatches[idx].tag = mTagCounter;
            mWatches[idx].manager = this;
            mWatches[idx].watch = watch;

            dbus_watch_set_data(watch, &(mWatches[idx]), NULL);

            return mTagCounter;
        }
    }

    return 0;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes a watch from our internal array
 *
 *
 *  @param[in]  tag         The tag number of the watch to remove
 */
void DbusWatches::deleteWatch(uint64_t tag)
{
    const size_t idx = static_cast<size_t>(tag % mMaxWatches);

    // clean up the dup'd file descriptor, it may already have been closed
    // on the detach call so it's not an error if it's not open here
    if (mWatches[idx].fd >= 0)
    {
        if (TEMP_FAILURE_RETRY(close(mWatches[idx].fd)) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to close dup'd file descriptor");
        }

        mWatches[idx].fd = -1;
    }

    // sanity check and clean up the rest
    if (mWatches[idx].tag != tag)
    {
        AI_LOG_ERROR("invalid tag value (tag=%" PRIu64 ")", tag);
        return;
    }
    else if (mWatches[idx].watch == nullptr)
    {
        AI_LOG_ERROR("trying to delete a watch that doesn't exist (tag=%" PRIu64 ")",
                     tag);
        return;
    }

    mWatches[idx].tag = 0;
    mWatches[idx].watch = nullptr;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback from dbus to add a new watch to poll on.
 *
 *  libdus should call this function from the dispatch loop thread.  We are
 *  responsible for adding the watch file descriptor to the poll loop with
 *  the given watch flags.  When we're woken by one of these we then need to
 *  call the watch's dispatch function (called in processEvent()).
 *
 *  @param[in]  watch       The dbus watch object to add
 *
 *  @return TRUE on success and FALSE on failure.
 */
dbus_bool_t DbusWatches::addWatch(DBusWatch *watch)
{
    AI_LOG_FN_ENTRY();

    // debugging check to ensure we're being called from the correct thread
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // Get the fd of the watch being removed
    int fd_ = dbus_watch_get_unix_fd(watch);
    if (fd_ < 0)
    {
        AI_LOG_ERROR_EXIT("watch has invalid fd");
        return FALSE;
    }

    // dup the file descriptor, we poll on that one rather the one provided by
    // libdbus as it tends to match multiple watches to the same fd, this way
    // we can have a unique fd and events per watch
    int duppedFd = fcntl(fd_, F_DUPFD_CLOEXEC, 3);
    if (duppedFd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to dup the file descriptor");
        return FALSE;
    }

    // create the watch which just adds it to our internal array and returns
    // the tag to access it
    uint64_t tag = createWatch(watch, duppedFd);
    if (tag == 0)
    {
        close(duppedFd);
        AI_LOG_ERROR_EXIT("failed to create the watch");
        return FALSE;
    }

    // convert dbus watch flags to epoll event flags
    uint32_t epollFlags = 0;
    if (dbus_watch_get_enabled(watch))
    {
        unsigned int watchFlags = dbus_watch_get_flags(watch);
        if (watchFlags & DBUS_WATCH_READABLE)
            epollFlags |= EPOLLIN;
        if (watchFlags & DBUS_WATCH_WRITABLE)
            epollFlags |= EPOLLOUT;
        if (watchFlags & DBUS_WATCH_HANGUP)
            epollFlags |= EPOLLHUP;
    }

    // install the dupped fd into the epoll loop
    if (epollFlags != 0)
    {
        struct epoll_event epollEvent;
        epollEvent.events = epollFlags;
        epollEvent.data.u64 = tag;

        if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, duppedFd, &epollEvent) != 0)
        {
            AI_LOG_SYS_ERROR_EXIT(errno, "failed to add watch to epoll");
            return FALSE;
        }
    }

    AI_LOG_FN_EXIT();
    return TRUE;
}

dbus_bool_t DbusWatches::addWatchCb(DBusWatch *watch, void *userData)
{
    DbusWatches* self = reinterpret_cast<DbusWatches*>(userData);
    return self->addWatch(watch);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes the watch from the epoll loop and our local array
 *
 *  libdus should call this function from the dispatch loop thread.  We check
 *  whether the watch has been added, and if so we remove it from epoll.
 *
 *  @param[in]  watch       The dbus watch object to remove
 *
 */
void DbusWatches::removeWatch(DBusWatch *watch)
{
    AI_LOG_FN_ENTRY();

    // debugging check to ensure we're being called from the correct thread
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // get the data which contains the dupped fd we're actually polling on
    WatchEntry* entry = reinterpret_cast<WatchEntry*>(dbus_watch_get_data(watch));
    if (!entry || (entry->manager != this) || (entry->watch != watch))
    {
        AI_LOG_ERROR_EXIT("invalid watch data entry?");
        return;
    }

    // remove the fd from the poll, note the fd may have already been deleted
    // from epoll by the toggleWatch() method, so we ignore the ENOENT error
    if ((epoll_ctl(mEpollFd, EPOLL_CTL_DEL, entry->fd, NULL) != 0) &&
        (errno != ENOENT))
    {
        AI_LOG_SYS_ERROR(errno, "failed to delete watch from epoll");
    }

    // free the watch entry
    deleteWatch(entry->tag);

    // clear the data of the watch
    dbus_watch_set_data(watch, NULL, NULL);

    AI_LOG_FN_EXIT();
}

void DbusWatches::removeWatchCb(DBusWatch *watch, void *userData)
{
    DbusWatches* self = reinterpret_cast<DbusWatches*>(userData);
    self->removeWatch(watch);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Toggles the watch flags on the given watch
 *
 *  libdus should call this function from the dispatch loop thread.  This just
 *  changes the epoll event flags to match the new dbus watch flags for the
 *  given watch.
 *
 *  @param[in]  watch       The dbus watch object to add
 *
 */
void DbusWatches::toggleWatch(DBusWatch *watch)
{
    AI_LOG_FN_ENTRY();

    // debugging check to ensure we're being called from the correct thread
#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mExpectedThreadId != std::this_thread::get_id())
    {
        AI_LOG_FATAL("called from wrong thread!");
    }
#endif

    // get the data which contains the dupped fd we're actually polling on
    WatchEntry* entry = reinterpret_cast<WatchEntry*>(dbus_watch_get_data(watch));
    if (!entry || (entry->manager != this) || (entry->watch != watch))
    {
        AI_LOG_ERROR_EXIT("invalid watch data entry?");
        return;
    }

    // convert dbus watch flags to epoll event flags
    uint32_t epollFlags = 0;
    if (dbus_watch_get_enabled(watch))
    {
        unsigned int watchFlags = dbus_watch_get_flags(watch);
        if (watchFlags & DBUS_WATCH_READABLE)
            epollFlags |= EPOLLIN;
        if (watchFlags & DBUS_WATCH_WRITABLE)
            epollFlags |= EPOLLOUT;
        if (watchFlags & DBUS_WATCH_HANGUP)
            epollFlags |= EPOLLHUP;
    }

    // remove the watch if the event flags are empty
    if (epollFlags == 0)
    {
        if ((epoll_ctl(mEpollFd, EPOLL_CTL_DEL, entry->fd, NULL) != 0) &&
            (errno != ENOENT))
        {
            AI_LOG_SYS_ERROR(errno, "failed to delete watch from epoll");
        }
    }
    else
    {
        // add or modify the watch if the flags are not empty
        struct epoll_event epollEvent;
        epollEvent.events = epollFlags;
        epollEvent.data.u64 = entry->tag;

        if (epoll_ctl(mEpollFd, EPOLL_CTL_MOD, entry->fd, &epollEvent) != 0)
        {
            // ENOENT indicates that the fd is not in the epoll loop, this is
            // not an error as it could have been toggled off earlier, so just
            // add this item now
            if (errno == ENOENT)
            {
                if (epoll_ctl(mEpollFd, EPOLL_CTL_ADD, entry->fd, &epollEvent) != 0)
                {
                    AI_LOG_SYS_ERROR(errno, "failed to add watch from epoll");
                }
            }
            else
            {
                AI_LOG_SYS_ERROR(errno, "failed to modifiy watch from epoll");
            }
        }
    }

    AI_LOG_FN_EXIT();
}

void DbusWatches::toggleWatchCb(DBusWatch *watch, void *userData)
{
    DbusWatches* self = reinterpret_cast<DbusWatches*>(userData);
    self->toggleWatch(watch);
}

