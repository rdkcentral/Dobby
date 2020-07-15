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
 * DbusEventDispatcher.h
 *
 */

#ifndef AI_IPC_DBUSEVENTDISPATCHER_H
#define AI_IPC_DBUSEVENTDISPATCHER_H

#include <dbus/dbus.h>

#include <thread>
#include <cstdint>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace AI_IPC
{

// -----------------------------------------------------------------------------
/**
 *  @class DbusEventDispatcher
 *  @brief Event dispatcher loop, runs the thread that polls on the dbus fds
 *
 *  This class installs handlers for the four types of dbus events; watches,
 *  timeouts, wake-ups and dispatches.  Watches and timeouts are a bit tricky
 *  and are handled by the DbusWatches and DbusTimeouts objects respectively,
 *  however those objects still have a corresponding fd that are added to the
 *  poll loop managed by this dispatchers.
 *
 *  In addition the dispatcher has an API to allow you to call arbitary
 *  functions in the context of it's dispatcher thread.  This is used by the
 *  higher level DbusConnection object to make libdbus API calls on the
 *  connection from the dispatch thread, and thereby avoid the many varied
 *  race conditions in the dbus library.
 *
 *
 */
class DbusEventDispatcher
{
public:
    DbusEventDispatcher();

    ~DbusEventDispatcher();

public:
    void startEventDispatcher(DBusConnection *connection);

    void stopEventDispatcher();

public:
    template< class Function, class... Args >
    bool callInEventLoop(Function&& f, Args&&... args)
    {
        return this->callInEventLoopImpl(std::bind(std::forward<Function>(f),
                                                   std::forward<Args>(args)...));
    }

    template< class Function >
    bool callInEventLoop(Function func)
    {
        return this->callInEventLoopImpl(func);
    }

private:
    void eventDispatcherThreadFn();

    void cleanupAllEvents();

private:
    static void dispatchStatusCb(DBusConnection *connection, DBusDispatchStatus status, void *userData);

    static void wakeUpCb(void *userData);

private:
    DBusConnection* mDbusConnection;

    std::thread mEventDispatcherThread;

    int mDeathEventFd;
    int mWakeupEventFd;
    int mDispatchEventFd;

private:
    bool callInEventLoopImpl(const std::function<void()>& func);

    uint64_t mCallCounter;
    std::mutex mCallLock;
    std::condition_variable mCallCondVar;
    std::queue< std::pair<uint64_t, std::function<void()>> > mCallQueue;

};

} // namespace AI_IPC

#endif // !defined(AI_IPC_DBUSEVENTDISPATCHER_H)
