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
/*
 *  DbusConnection.h
 *
 */
#ifndef AI_IPC_DBUSCONNECTION_H
#define AI_IPC_DBUSCONNECTION_H

#include "DbusEventDispatcher.h"
#include <ConditionVariable.h>
#include <Mutex.h>
#include <SpinLock.h>

#include <map>
#include <mutex>
#include <atomic>
#include <string>
#include <cstdint>
#include <memory>
#include <functional>
#include <condition_variable>

#include <sys/types.h>

#include <dbus/dbus.h>


namespace AI_IPC
{


// -----------------------------------------------------------------------------
/**
 *  @class DbusConnection
 *  @brief Wraps a dbus connection pointer and runs the dispatch loop for it.
 *
 *  This class was added to force all libdus calls that take a DBusConnection*
 *  pointer as an argument to be routed through a single event loop thread.
 *
 *  
 *
 */
class DbusConnection
{
public:
    DbusConnection();

    ~DbusConnection();

public:
    bool connect(DBusBusType busType, const std::string& serviceName = std::string());

    bool connect(const std::string& address, const std::string& serviceName = std::string());

    void disconnect();

public:
    void registerMessageHandler(const std::function<DBusHandlerResult(DBusMessage*)>& handler);

public:
    bool sendMessageNoReply(DBusMessage *msg);

    uint64_t sendMessageWithReply(DBusMessage *msg, int timeout);

    DBusMessage* getReply(uint64_t token);

    bool cancelReply(uint64_t token);


    bool nameHasOwner(const std::string& name);

    uid_t getUnixUser(const std::string& name);


    bool addMatch(const std::string& rule);

    bool removeMatch(const std::string& rule);


    bool flushConnection();
    

private:
    DBusConnection *mDbusConnection;

    DbusEventDispatcher mEventDispacher;

    bool completeConnect(DBusConnection* conn, const std::string& serviceName);

private:
    static DBusHandlerResult handleDbusMessageCb(DBusConnection *connection,
                                                 DBusMessage *message,
                                                 void *userData);

    typedef std::function<DBusHandlerResult(DBusMessage*)> MessageHandler;
    MessageHandler mHandler;

    AICommon::Spinlock mHandlerLock;

private:
    bool reserveServiceName(DBusConnection *dbusConnection,
                            const std::string& name) const;

    std::string mServiceName;

private:
    static void pendingCallNotifyFcn(DBusPendingCall *pending, void *userData);

    static void pendingCallFreeFcn(void *userData);

private:
    typedef struct _ReplyContext
    {
        uint64_t token;
        DbusConnection* conn;
    } ReplyContext;

    std::atomic<uint64_t> mTokenCounter;

    AICommon::Mutex mRepliesLock;
    AICommon::ConditionVariable mRepliesCondVar;
    std::map<uint64_t, DBusMessage*> mReplies;
};


} // namespace AI_IPC

#endif // !defined(AI_IPC_DBUSCONNECTION_H)
