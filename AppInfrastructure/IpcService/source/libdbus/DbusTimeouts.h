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
 *  DbusTimeouts.h
 *
 */
#ifndef AI_IPC_DBUSTIMEOUTS_H
#define AI_IPC_DBUSTIMEOUTS_H

#include <list>
#include <thread>

#include <time.h>

#include <dbus/dbus.h>


namespace AI_IPC
{


// -----------------------------------------------------------------------------
/**
 *  @class DbusTimeouts
 *  @brief Object that manages the timeouts for a given dbus connection.
 *
 *  @warning This class is not thread safe, it is designed to only be called
 *  from one thread which is the same thread the libdbus callbacks will be
 *  called from.  On debug builds an error will be reported if called from
 *  any other thread.
 *
 *  Internally it creates an timerfd object and a sorted list of expiry times
 *  matched to dbus timeout objects.  The expiry item on the head of the list
 *  is programmed into the timerfd so the poll loop will wake up when the
 *  timer expires.
 *
 *  Although the code is not thread safe (by design) it handles reentrant calls
 *  to the installed dbus callbacks while in the processing loop.
 *
 */
class DbusTimeouts
{
public:
    DbusTimeouts(DBusConnection *conn);
    ~DbusTimeouts();

public:
    int fd() const;
    void processEvent(unsigned int pollEvents);

private:
    static dbus_bool_t addTimeOutCb(DBusTimeout *timeout, void *userData);
    static void toggleTimeOutCb(DBusTimeout *timeout, void *userData);
    static void removeTimeOutCb(DBusTimeout *timeout, void *userData);

    dbus_bool_t addTimeOut(DBusTimeout *timeout);
    void toggleTimeOut(DBusTimeout *timeout);
    void removeTimeOut(DBusTimeout *timeout);

private:
    struct timespec calcAbsTime(const struct timespec& base,
                                int milliseconds) const;

    void updateTimerFd() const;

    inline bool hasExpired(const struct timespec& expiryTime,
                           const struct timespec& currentTime) const;

private:
    int mTimerFd;
    DBusConnection* const mDbusConnection;
    bool mWithinEventHandler;

private:
    typedef struct _TimeoutEntry
    {
        struct timespec expiry;
        DBusTimeout* timeout;

        bool operator< (const struct _TimeoutEntry& rhs) const
        {
            return (expiry.tv_sec < rhs.expiry.tv_sec) ||
                   ((expiry.tv_sec == rhs.expiry.tv_sec) &&
                    (expiry.tv_nsec < rhs.expiry.tv_nsec));
        }

    } TimeoutEntry;

    std::list<TimeoutEntry> mTimeouts;

#if (AI_BUILD_TYPE == AI_DEBUG)
private:
    const std::thread::id mExpectedThreadId;
#endif
};



} // namespace AI_IPC

#endif // !defined(AI_IPC_DBUSTIMEOUTS_H)
