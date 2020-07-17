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
 *  DbusWatches.h
 *
 */
#ifndef AI_IPC_DBUSWATCHES_H
#define AI_IPC_DBUSWATCHES_H

#include <thread>
#include <cstdint>

#include <sys/epoll.h>

#include <dbus/dbus.h>


namespace AI_IPC
{


// -----------------------------------------------------------------------------
/**
 *  @class DbusWatches
 *  @brief Utility object to handle installing / removing dbus watches from the
 *  poll loop.
 *
 *  @warning This class is not thread safe, it is designed to only be called
 *  from one thread which is the same thread the libdbus callbacks will be
 *  called from.  On debug builds an error will be reported if called from
 *  an invalid thread.
 *
 *  Internally it creates an epoll object that has watches (which are just fds)
 *  added to / removed from it.  The epoll fd is returned by this object, and
 *  the dispatch poll loop will poll on this.  So in effect this is a second
 *  level of poll file descriptors.
 *
 *  This code automatically installs the dbus callbacks on the connection at
 *  construction time, and removes the callbacks at destruction.
 *
 *  When watches are added we dup the file descriptors, and add the dupped fd
 *  to epoll.  The reason for this is that libdbus tends to have more than
 *  one watch associated with a single fd and you can't add the same fd more
 *  once to an epoll event loop.  By dup'ing the file descriptors it means we
 *  can have one epoll event entry per dbus watch.
 *
 */
class DbusWatches
{
public:
    DbusWatches(DBusConnection *conn);
    ~DbusWatches();

public:
    int fd() const;
    void processEvent(unsigned int pollEvents);

private:
    static dbus_bool_t addWatchCb(DBusWatch *watch, void *userData);
    static void toggleWatchCb(DBusWatch *watch, void *userData);
    static void removeWatchCb(DBusWatch *watch, void *userData);

    dbus_bool_t addWatch(DBusWatch *watch);
    void toggleWatch(DBusWatch *watch);
    void removeWatch(DBusWatch *watch);

    uint64_t createWatch(DBusWatch *watch, int duppedFd);
    void deleteWatch(uint64_t tag);

private:
    DBusConnection* const mDbusConnection;
    int mEpollFd;

private:
    static const unsigned mMaxWatches = 128;
    uint64_t mTagCounter;

    typedef struct _WatchEntry
    {
        int fd;
        uint64_t tag;
        DBusWatch* watch;
        DbusWatches* manager;
    } WatchEntry;
    WatchEntry mWatches[mMaxWatches];

private:
    struct epoll_event mEpollEvents[mMaxWatches];

#if (AI_BUILD_TYPE == AI_DEBUG)
private:
    const std::thread::id mExpectedThreadId;
#endif
};



} // namespace AI_IPC

#endif // !defined(AI_IPC_DBUSWATCHES_H)
