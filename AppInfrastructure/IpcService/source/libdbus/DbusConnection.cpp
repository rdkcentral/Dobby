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
 *  DbusConnection.cpp
 *
 */
#include "DbusConnection.h"

#include <Logging.h>
#include <cinttypes>

using namespace AI_IPC;


DbusConnection::DbusConnection()
    : mDbusConnection(nullptr)
    , mHandler(nullptr)
    , mTokenCounter(1)
{
    // we setup multi-thread access, however we may not need it as we bounce
    // all the dbus calls that use the connection object into a single thread
    // dispatch event loop
    if (!dbus_threads_init_default())
    {
        AI_LOG_FATAL("dbus_threads_init_default failed");
    }
}

DbusConnection::~DbusConnection()
{
    // check if we're being destructed while connected ... this is a bug
    if (mDbusConnection)
    {
        AI_LOG_ERROR("destructed while connected, forcing disconnect");

        disconnect();
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to reserve a service name on dbus.
 *
 *  Service names must be unique (per dbus) so if the name is already owned
 *  by another dbus client then this function will fail.
 *
 *  @param[in]  dbusConnection  The dbus connection we want to reserve the
 *                              name on.
 *  @param[in]  name            The name we're trying to reserve.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::reserveServiceName(DBusConnection *dbusConnection,
                                        const std::string& name) const
{
    AI_LOG_FN_ENTRY();

    DBusError error;
    dbus_error_init(&error);

    dbus_bool_t ret = dbus_bus_name_has_owner(dbusConnection, name.c_str(), &error);
    if (dbus_error_is_set(&error))
    {
        AI_LOG_ERROR_EXIT("error in checking if there is an owner for '%s' - %s",
                          name.c_str(), error.message ? error.message : "Unknown error");
        dbus_error_free(&error);
        return false;
    }

    if (ret != FALSE)
    {
        AI_LOG_ERROR_EXIT("bus name '%s' already reserved", name.c_str());
        return false;
    }
    else
    {
        AI_LOG_INFO("Bus name %s doesn't have an owner, reserving it...", name.c_str());

        int result = dbus_bus_request_name(dbusConnection, name.c_str(),
                                           DBUS_NAME_FLAG_DO_NOT_QUEUE, &error);
        if (dbus_error_is_set(&error))
        {
            AI_LOG_ERROR_EXIT("error requesting bus name '%s' - %s",
                              name.c_str(), error.message ? error.message : "Unknown error");
            dbus_error_free(&error);
            return false;
        }
        else if (result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER)
        {
            AI_LOG_INFO("primary ownership not granted for bus: %s",
                        name.c_str());
        }
        else
        {
            AI_LOG_INFO("DBus bus name %s is in use for AI RPC service",
                        name.c_str());
        }
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to connect to one of the known buses and optionally reserve
 *  the given service name.
 *
 *
 *  @param[in]  busType         Must be either; DBUS_BUS_SESSION or DBUS_BUS_SYSTEM
 *  @param[in]  serviceName     Optional service name to use for the connection.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::connect(DBusBusType busType,
                             const std::string& serviceName /*= std::string()*/)
{
    DBusError error;
    dbus_error_init(&error);

    DBusConnection* conn = dbus_bus_get_private(busType, &error);
    if ((conn == nullptr) || dbus_error_is_set(&error))
    {
        AI_LOG_ERROR_EXIT("error connecting to the bus - %s",
                          error.message ? error.message : "Unknown error");
        dbus_error_free(&error);
        return false;
    }

    return completeConnect(conn, serviceName);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Attempts to connect to the bus and optionally reserve the given
 *  service name.
 *
 *
 *  @param[in]  address         The dbus address to connected to.
 *  @param[in]  serviceName     Optional service name to use for the connection.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::connect(const std::string& address,
                             const std::string& serviceName /*= std::string()*/)
{
    DBusError error;
    dbus_error_init(&error);

    DBusConnection* conn = dbus_connection_open_private(address.c_str(), &error);
    if ((conn == nullptr) || dbus_error_is_set(&error))
    {
        AI_LOG_ERROR_EXIT("error connecting to the daemon bus - %s",
                          error.message ? error.message : "Unknown error");
        dbus_error_free(&error);
        return false;
    }

    return completeConnect(conn, serviceName);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Completes the initialisation of the dbus connection.
 *
 *  If an error occurs the supplied connection will be closed and unref'ed.
 *
 *  @param[in]  conn            Pointer to the newly opened dbus connection.
 *  @param[in]  serviceName     Service name to use for the connection.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::completeConnect(DBusConnection* conn,
                                     const std::string& serviceName)
{
    AI_LOG_FN_ENTRY();

    DBusError error;
    dbus_error_init(&error);

    // we never want to exit on disconnect, this should be the default, but
    // just in case force it to false here
    dbus_connection_set_exit_on_disconnect(conn, FALSE);

    // register ourselves on the bus
    dbus_bool_t ret = dbus_bus_register(conn, &error);
    if ((ret == FALSE) || dbus_error_is_set(&error))
    {
        AI_LOG_ERROR_EXIT("dbus_bus_register failed - %s",
                          error.message ? error.message : "Unknown error");
        dbus_error_free(&error);

        dbus_connection_close(conn);
        dbus_connection_unref(conn);

        return false;
    }

    // if the caller supplied a service name then try and claim it
    if (!serviceName.empty())
    {
        if (reserveServiceName(conn, serviceName))
        {
            mServiceName = serviceName;
        }
        else
        {
            dbus_connection_close(conn);
            dbus_connection_unref(conn);

            AI_LOG_FN_EXIT();
            return false;
        }
    }

    // save the connection before installing the filter and starting the
    // dispatcher
    mDbusConnection = conn;

    // install a message filter, which is our callback point for signalling to
    // clients that a method call or signal has arrived
    if (dbus_connection_add_filter(mDbusConnection, handleDbusMessageCb, this, NULL) != TRUE)
    {
        AI_LOG_ERROR("failed to install dbus message filter, this is quite bad");
    }

    // start the dispatch thread / loop
    mEventDispacher.startEventDispatcher(mDbusConnection);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Disconnect from the bus.
 *
 *  This will also cancel any pending method calls.
 *
 *
 */
void DbusConnection::disconnect()
{
    AI_LOG_FN_ENTRY();

    // sanity check we're actually connected
    if (mDbusConnection == nullptr)
    {
        AI_LOG_ERROR_EXIT("not connected");
        return;
    }

    // stop the dispatcher, this will also remove any handler callbacks
    mEventDispacher.stopEventDispatcher();

    // remove the message filter
    dbus_connection_remove_filter(mDbusConnection, handleDbusMessageCb, this);

    // if we registered ourselves with a service name then release it now
    if (!mServiceName.empty())
    {
        DBusError error;
        dbus_error_init( &error );

        int res = dbus_bus_release_name(mDbusConnection, mServiceName.c_str(), &error);
        if ((res == -1) || dbus_error_is_set(&error))
        {
            AI_LOG_ERROR("dbus_bus_release_name failed - %s", error.message);
            dbus_error_free(&error);
        }

        mServiceName.clear();
    }

    // flush the dbus connection before closing
    dbus_connection_flush(mDbusConnection);

    // make sure to close the connection since we opened it in private mode
    dbus_connection_close(mDbusConnection);

    // release the dbus connection
    dbus_connection_unref(mDbusConnection);

    // reset the pointer
    mDbusConnection = nullptr;


    // free any reply objects which may have been put in the queue but never
    // 'got' by the caller
    std::unique_lock<AICommon::Mutex> locker(mRepliesLock);

    if (!mReplies.empty())
    {
        AI_LOG_WARN("outstanding replies left over, cleaning up");

        for (std::pair<const uint64_t, DBusMessage*>& reply : mReplies)
        {
            if (reply.second != nullptr)
                dbus_message_unref(reply.second);
        }

        mReplies.clear();
    }


    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback from the libdbus in the context of the event / dispatcher
 *  thread.
 *
 *  This callback is installed right after we've connected, we hook this point
 *  so we can pass it onto the handler installed with @a registerMessageHandler
 *
 *
 *  @param[in]  connection      The connection dbus connection.
 *  @param[in]  message         The message received.
 *  @param[in]  userData        User data, in our case a pointer to this
 *
 *  @return if no handler is installed then DBUS_HANDLER_RESULT_NOT_YET_HANDLED
 *  is returned, otherwise the result returned by installed handler.
 */
DBusHandlerResult DbusConnection::handleDbusMessageCb(DBusConnection *connection,
                                                      DBusMessage *message,
                                                      void *userData)
{
    DbusConnection* self = reinterpret_cast<DbusConnection*>(userData);
    if (!self || (self->mDbusConnection != connection))
    {
        AI_LOG_FATAL("invalid filter callback data");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    std::unique_lock<AICommon::Spinlock> locker(self->mHandlerLock);
    const MessageHandler handler = self->mHandler;
    locker.unlock();

    if (handler == nullptr)
    {
        AI_LOG_DEBUG("no handler installed for dbus messages");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    return handler(message);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Registers a handler to be called when any message (method call or
 *  signal) is received.
 *
 *  Only one handler can be installed at a time, to remove the handler pass
 *  nullptr.
 *
 *  @param[in]  handler         Function to call on receiving a message
 *
 */
void DbusConnection::registerMessageHandler(const std::function<DBusHandlerResult(DBusMessage*)>& handler)
{
    // save the handler
    std::unique_lock<AICommon::Spinlock> locker(mHandlerLock);
    mHandler = handler;
    locker.unlock();

    // flush the connection to ensure if the old handler won't be called after
    // we return
    flushConnection();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback from libdus when either a reply is received for a pending
 *  call or the timeout expires.
 *
 *  The timeout for the call is set when the method call request is queued
 *  (i.e. with the @a sendMessageWithReply() method).
 *
 *  This is the point were we get the reply message (which could be a timeout
 *  message) and assign it against the token value stored in the map.
 *
 *
 *  @param[in]  pending     The pending call object that has completed
 *  @param[in]  userData    User data, in our case a pointer to a ReplyContext
 *                          object.
 */
void DbusConnection::pendingCallNotifyFcn(DBusPendingCall *pending, void *userData)
{
    AI_LOG_FN_ENTRY();

    const ReplyContext* ctx = reinterpret_cast<ReplyContext*>(userData);
    if (!ctx || !ctx->conn)
    {
        AI_LOG_ERROR_EXIT("invalid context pointer");
        return;
    }

    // check if the pending call has completed
    if (dbus_pending_call_get_completed(pending) == FALSE)
    {
        AI_LOG_ERROR_EXIT("not complete");
        return;
    }

    // get the reply object
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    if (reply == nullptr)
    {
        AI_LOG_ERROR_EXIT("odd, no reply object");
        return;
    }

    // find the token in the reply map
    std::lock_guard<AICommon::Mutex> locker(ctx->conn->mRepliesLock);

    std::map<uint64_t, DBusMessage*>::iterator it = ctx->conn->mReplies.find(ctx->token);
    if (it == ctx->conn->mReplies.end())
    {
        // it's not (necessarily) an error if the token is not in the map, it
        // could just mean that someone has called the cancelReply(...) method
        // for this token, so just free the reply and then exit
        dbus_message_unref(reply);
        return;
    }

    // store the reply and then wake all the listeners
    it->second = reply;
    ctx->conn->mRepliesCondVar.notify_all();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback from libdus when a pending call notifier is being destroyed
 *  and we should clean up the context data.
 *
 *
 *
 *  @param[in]  userData    Pointer to the data to free.
 */
void DbusConnection::pendingCallFreeFcn(void *userData)
{
    const ReplyContext* ctx = reinterpret_cast<ReplyContext*>(userData);
    if (ctx)
    {
        AI_LOG_DEBUG("deleting reply object for token %" PRIu64 "", ctx->token);
        delete ctx;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends a dbus message out the connection
 *
 *  This calls dbus_connection_send_with_reply(...), from the context of the
 *  dispatch thread loop.  If that succeeds it installs a notifier on the
 *  pending call result and allocates a reply token that the caller should
 *  use for getting the reply.
 *
 *  The returned token MUST be consumed by calling either @a getReply or
 *  @a cancelReply, if this is not done then the reply will be stored until
 *  the connection is disconnected.
 *
 *
 *  @param[in]  msg         The message to send, it should be a method call.
 *  @param[in]  timeout     The timeout in milliseconds to wait for a response,
 *                          if -1 then a 'sensible' default is used, typically
 *                          30 seconds.
 *
 *  @return a unique 64-bit token value to use for getting the reply message,
 *  on error 0 will be returned.
 */
uint64_t DbusConnection::sendMessageWithReply(DBusMessage *msg, int timeout)
{
    uint64_t replyToken = 0;

    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        DBusPendingCall* pendingCall = nullptr;

        if (dbus_connection_send_with_reply(mDbusConnection, msg, &pendingCall, timeout) != TRUE)
        {
            AI_LOG_ERROR("dbus_connection_send_with_reply failed");
        }
        else if (pendingCall == nullptr)
        {
            AI_LOG_ERROR("no pending call object returned");
        }
        else
        {
            replyToken = mTokenCounter++;

            // add the token to the map with a null reply object
            {
                std::lock_guard<AICommon::Mutex> locker(mRepliesLock);
                mReplies.emplace(replyToken, nullptr);
            }

            // install a notification function with some data so when a reply
            // is received or a timeout occurs we can wake up the getReply()
            // function
            ReplyContext* ctx = new ReplyContext;
            ctx->conn = this;
            ctx->token = replyToken;

            if (dbus_pending_call_set_notify(pendingCall, pendingCallNotifyFcn,
                                             ctx, pendingCallFreeFcn) != TRUE)
            {
                dbus_pending_call_cancel(pendingCall);
                delete ctx;

                AI_LOG_ERROR("failed to install notify function");

                std::lock_guard<AICommon::Mutex> locker(mRepliesLock);
                mReplies.erase(replyToken);
                replyToken = 0;
            }

            dbus_pending_call_unref(pendingCall);
        }
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
    }

    return replyToken;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends a message on the connection without expecting a reply
 *
 *  This is typically used for emitting a signal, however can be used for a
 *  method call that is not expecting a reply.
 *
 *
 *  @param[in]  msg     The message to send out the bus.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::sendMessageNoReply(DBusMessage *msg)
{
    dbus_bool_t result;

    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        result = dbus_connection_send(mDbusConnection, msg, NULL);
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
        return false;
    }
    else if (result == FALSE)
    {
        AI_LOG_ERROR("dbus_connection_send failed");
        return false;
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the reply for the given request.
 *
 *  This method blocks until a reply or timeout has been received.  The timeout
 *  time is set when the messag was originally sent (by sendMessageWithReply).
 *  However there is a maximum timeout value of 120 seconds, this should never
 *  be hit in normal operation, if it is then it indicates a problem with the
 *  timeout code in libdbus.
 *
 *  @param[in]  token       The token returned by the sendMessageWithReply(...)
 *                          method.
 *
 *  @return on success the reply message, it is the callers responsbility to
 *  call dbus_message_unref on the message object.  On failure a nullptr is
 *  returned.
 */
DBusMessage* DbusConnection::getReply(uint64_t token)
{
    static const std::chrono::seconds maxTimeout(120);

    std::unique_lock<AICommon::Mutex> locker(mRepliesLock);

    // wait until we've got the reply token in the map, note there is no
    // timeout here, we trust libdbus to correctly signal our callback for
    // all cases including errors (i.e. timeouts)
    std::map<uint64_t, DBusMessage*>::iterator it = mReplies.find(token);
    while ((it != mReplies.end()) && (it->second == nullptr))
    {
        if (mRepliesCondVar.wait_for(locker, maxTimeout) == std::cv_status::timeout)
        {
            AI_LOG_ERROR("exceed maximum time out waiting for reply (%" PRId64 " seconds timeout)", maxTimeout.count());

            // remove the token from the map if it still exists
            it = mReplies.find(token);
            if (it != mReplies.end())
                mReplies.erase(it);

            return nullptr;
        }

        it = mReplies.find(token);
    }

    // check if the token was actually in the map
    if (it == mReplies.end())
    {
        AI_LOG_ERROR("token %" PRIu64 " is invalid", token);
        return nullptr;
    }

    // get the reply object and then remove from the map
    DBusMessage* reply = it->second;
    mReplies.erase(it);

    return reply;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Cancels waiting for the reply.
 *
 *  This frees the memory reserved for storing the reply.
 *
 *  @param[in]  token       The token returned by the sendMessageWithReply(...)
 *                          method.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::cancelReply(uint64_t token)
{
    std::unique_lock<AICommon::Mutex> locker(mRepliesLock);

    // find the token and remove it
    std::map<uint64_t, DBusMessage*>::iterator it = mReplies.find(token);
    if (it == mReplies.end())
    {
        AI_LOG_ERROR("token %" PRIu64 " is not in the map", token);
        return false;
    }

    // need to free the reply message if present
    if (it->second != nullptr)
        dbus_message_unref(it->second);

    // remove the reply token from the map
    mReplies.erase(it);

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns true if the supplied name exists on the bus
 *
 *  This calls dbus_bus_name_has_owner(...), from the context of the dispatch
 *  thread loop.
 *
 *  @param[in]  name        The service name to search for.
 *
 *  @return true if the name exists on the bus, false on error or if the name
 *  is not on the bus.
 */
bool DbusConnection::nameHasOwner(const std::string& name)
{
    dbus_bool_t result;

    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        DBusError error;
        dbus_error_init(&error);

        result = dbus_bus_name_has_owner(mDbusConnection, name.c_str(), &error);
        if (dbus_error_is_set(&error))
        {
            AI_LOG_ERROR("dbus_bus_name_has_owner failed - %s", error.message);
            dbus_error_free(&error);
        }
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
        return false;
    }

    return (result == TRUE);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the unix user id of the named client.
 *
 *  This calls dbus_bus_get_unix_user(...), from the context of the dispatch
 *  thread loop.
 *
 *  @param[in]  name        The client name to get the user id of.
 *
 *  @return positive user id if the name exists on the bus, -1 on error.
 */
uid_t DbusConnection::getUnixUser(const std::string& name)
{
    uid_t uid = -1;

    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        DBusError error;
        dbus_error_init( &error );

        unsigned long userId = dbus_bus_get_unix_user(mDbusConnection, name.c_str(), &error);
        if (dbus_error_is_set(&error) || (userId == (unsigned long)-1))
        {
            AI_LOG_ERROR("dbus_bus_get_unix_user failed: %s", error.message);
            dbus_error_free(&error);
        }
        else
        {
            uid = static_cast<uid_t>(userId);
            AI_LOG_DEBUG("Unix user ID retrieved %d", uid);
        }
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
        return static_cast<uid_t>(-1);
    }

    return uid;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a match rule for the connection
 *
 *  This calls dbus_bus_add_match(...), from the context of the dispatch
 *  thread loop.
 *
 *  @param[in]  rule        The dbus match rule to add
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::addMatch(const std::string& rule)
{
    bool result = false;

    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        DBusError error;
        dbus_error_init(&error);

        dbus_bus_add_match(mDbusConnection, rule.c_str(), &error);
        if (!dbus_error_is_set(&error))
        {
            result = true;
        }
        else
        {
            AI_LOG_ERROR("dbus_bus_add_match failed for \"%s\" (error: %s)",
                         rule.c_str(), error.message);
            dbus_error_free(&error);
            result = false;
        }
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
        return false;
    }

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes a match rule from the connection.
 *
 *  This calls dbus_bus_remove_match(...), from the context of the dispatch
 *  thread loop.
 *
 *  @param[in]  rule        The dbus match rule to remove.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::removeMatch(const std::string& rule)
{
    bool result = false;

    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        DBusError error;
        dbus_error_init(&error);

        dbus_bus_remove_match(mDbusConnection, rule.c_str(), &error);
        if (!dbus_error_is_set(&error))
        {
            result = true;
        }
        else
        {
            AI_LOG_ERROR("dbus_bus_remove_match failed for \"%s\" (error: %s)",
                         rule.c_str(), error.message);
            dbus_error_free(&error);
            result = false;
        }
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
        return false;
    }

    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Flushes the dbus connection.
 *
 *  This calls dbus_connection_flush(...), from the context of the dispatch
 *  thread loop.
 *
 *  @return true on success, false on failure.
 */
bool DbusConnection::flushConnection()
{
    // the following block of code is a lambda, basically a function that will
    // be executed in the thread of the dispatcher when we call
    // mEventDispacher.callInEventLoop()
    const std::function<void()> worker = [&]()
    {
        dbus_connection_flush(mDbusConnection);
    };

    // call the above lambda and get the result, this is a blocking call but
    // it will be executed within the dispatcher thread
    if (!mEventDispacher.callInEventLoop(worker))
    {
        AI_LOG_ERROR("failed to execute worker in dispatcher thread");
        return false;
    }

    return true;
}



