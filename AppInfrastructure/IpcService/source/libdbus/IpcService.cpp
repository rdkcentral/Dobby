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
 * IpcService.cpp
 *
 *  Created on: 5 Jun 2015
 *      Author: riyadh
 */

#include "IpcCommon.h"
#include "IpcService.h"
#include "IDbusServer.h"
#include "AsyncReplySender.h"
#include "AsyncReplyGetter.h"
#include "DbusEventDispatcher.h"
#include "DbusMessageParser.h"
#include "IpcUtilities.h"
#include "DbusEntitlements.h"
#include "DbusUserIdSenderIdCache.h"

#include <Common/Interface.h>
#include <Logging.h>

#include <dbus/dbus.h>

#include <boost/optional.hpp>
#include <boost/lexical_cast.hpp>

#include <thread>
#include <future>
#include <string>
#include <exception>
#include <stdexcept>
#include <list>
#include <mutex>
#include <set>


namespace {

std::string getMatchRule(const AI_IPC::RemoteEntry& entry)
{
    std::string matchRule;

    if( entry.type == AI_IPC::RemoteEntry::SIGNAL )
    {
        matchRule += "type='signal'";
    }
    else if( entry.type == AI_IPC::RemoteEntry::METHOD )
    {
        matchRule += "type='method_call'";
    }
    else
    {
        throw std::runtime_error("Entry has to be either method not signal");
    }

    if( !entry.interface.empty() )
    {
        matchRule += ",interface='"+ entry.interface +"'";
    }

    if( !entry.name.empty() )
    {
        matchRule += ",member='"+ entry.name +"'";
    }

    if( !entry.object.empty() )
    {
        matchRule += ",path='"+ entry.object +"'";
    }

    if( entry.type == AI_IPC::RemoteEntry::METHOD )
    {
        matchRule += ",destination='"+ entry.service +"'";
    }

    return matchRule;
}


bool validRemoteEntry(const AI_IPC::RemoteEntry& entry)
{
    return ( (!entry.name.empty()) && (!entry.interface.empty()) && (!entry.object.empty()) );
}

}

using namespace AI_IPC;

IpcService::IpcService(const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer, const std::string& serviceName, int defaultTimeoutMs /*= -1*/)
    : mDbusServer(dbusServer)
    , mServiceName(serviceName)
    , mHandlerDispatcher("AI_DBUS_DISPATCH")
    , mRunning(false)
    , mNextSignalHandlerRegId(1)
    , mDefaultTimeoutMs(defaultTimeoutMs)
#if (AI_BUILD_TYPE == AI_DEBUG)
    , mInMonitorMode(false)
    , mMonitorCb(nullptr)
#endif
    , mDbusEntitlementCheckNeeded(false)
{
    AI_LOG_FN_ENTRY();

    if ( !dbusServer || serviceName.empty() )
    {
        throw std::runtime_error("Invalid construction parameter for dbus service" );
    }

    std::string address = dbusServer->getBusAddress();

    if ( address.empty() )
    {
        throw std::runtime_error("Invalid dbus address" );
    }

    mDbusConnection = std::make_shared<DbusConnection>();
    if ( !mDbusConnection || !mDbusConnection->connect(address, serviceName) )
    {
        throw std::runtime_error( "Failed to connect to dbus" );
    }

    AI_LOG_FN_EXIT();
}

IpcService::IpcService( const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer,
                        const std::string& serviceName,
                        const std::shared_ptr<packagemanager::IPackageManager> &packageManager,
                        bool dbusEntitlementCheckNeeded /* = false*/,
                        int defaultTimeoutMs /*= -1*/ )
: IpcService(dbusServer, serviceName, defaultTimeoutMs)
{
    // not doing member initialization on these, as don't wan't to change the parameter list of the old ctor
    mDbusPackageEntitlements = std::make_shared<DbusEntitlements>(packageManager);

    mDbusUserIdSenderIdCache = std::make_shared<DbusUserIdSenderIdCache>(*this, mDbusPackageEntitlements);

    mDbusEntitlementCheckNeeded = dbusEntitlementCheckNeeded;
}

IpcService::IpcService(BusType busType, const std::string& serviceName, int defaultTimeoutMs /*= -1*/)
    : mDbusServer(nullptr)
    , mServiceName(serviceName)
    , mHandlerDispatcher("AI_DBUS_DISPATCH")
    , mRunning(false)
    , mNextSignalHandlerRegId(1)
    , mDefaultTimeoutMs(defaultTimeoutMs)
#if (AI_BUILD_TYPE == AI_DEBUG)
    , mInMonitorMode(false)
    , mMonitorCb(nullptr)
#endif
    , mDbusEntitlementCheckNeeded(false)
{
    AI_LOG_FN_ENTRY();

    if ( serviceName.empty() )
    {
        throw std::runtime_error( "Invalid construction parameter for dbus service" );
    }

    DBusBusType type;
    switch (busType)
    {
        case BusType::SessionBus:
            type = DBUS_BUS_SESSION;
            break;
        case BusType::SystemBus:
            type = DBUS_BUS_SYSTEM;
            break;
        default:
            throw std::runtime_error( "Invalid bus type" );
    }

    mDbusConnection = std::make_shared<DbusConnection>();
    if ( !mDbusConnection || !mDbusConnection->connect(type, serviceName) )
    {
        throw std::runtime_error( "Failed to connect to dbus" );
    }

    AI_LOG_FN_EXIT();
}

IpcService::IpcService(const std::string& dbusAddress, const std::string& serviceName, int defaultTimeoutMs)
    : mDbusServer(nullptr)
    , mServiceName(serviceName)
    , mHandlerDispatcher("AI_DBUS_DISPATCH")
    , mRunning(false)
    , mNextSignalHandlerRegId(1)
    , mDefaultTimeoutMs(defaultTimeoutMs)
#if (AI_BUILD_TYPE == AI_DEBUG)
    , mInMonitorMode(false)
    , mMonitorCb(nullptr)
#endif
    , mDbusEntitlementCheckNeeded(false)
{
    AI_LOG_FN_ENTRY();

    if (dbusAddress.empty())
    {
        throw std::runtime_error("Invalid address parameter for dbus service");
    }
    if (serviceName.empty())
    {
        throw std::runtime_error("Invalid construction parameter for dbus service");
    }

    mDbusConnection = std::make_shared<DbusConnection>();
    if (!mDbusConnection || !mDbusConnection->connect(dbusAddress, serviceName))
    {
        throw std::runtime_error("Failed to connect to dbus");
    }

    AI_LOG_FN_EXIT();
}

IpcService::~IpcService()
{
    AI_LOG_FN_ENTRY();

    stop();

    unregisterHandlers();

    if (mDbusConnection)
    {
        mDbusConnection->disconnect();
        mDbusConnection.reset();
    }

    AI_LOG_FN_EXIT();
}

std::shared_ptr<IAsyncReplyGetter> IpcService::invokeMethod(const Method& method, const VariantList& args, int timeoutMs /*= -1*/)
{
    AI_LOG_FN_ENTRY();

#if (AI_BUILD_TYPE == AI_DEBUG)
    if ( !mRunning )
    {
        AI_LOG_WARN("Trying to call a method without IpcService event loop running");
    }
#endif

    std::shared_ptr<IAsyncReplyGetter> replyGetter;

    if ( validRemoteEntry(method) )
    {
        DBusMessage *msg = dbus_message_new_method_call( method.service.c_str(), method.object.c_str(), method.interface.c_str(), method.name.c_str() );
        if ( msg != NULL )
        {
            if( appendArgsToDbusMsg(msg, args) )
            {
                // If the timeout is set to the default, override with the Service's default timeout
                if (timeoutMs == -1)
                {
                    timeoutMs = mDefaultTimeoutMs;
                }

                // Send the message and get a token to wait on for the reply
                uint64_t token = mDbusConnection->sendMessageWithReply(msg, timeoutMs);
                if (token != 0)
                {
                    replyGetter = std::make_shared<AsyncReplyGetter>(mDbusConnection, token);
                }
            }
            else
            {
                AI_LOG_ERROR("Unable to append arguments to dbus message");
            }

            dbus_message_unref(msg);
        }
        else
        {
            AI_LOG_ERROR( "Error: dbus_message_new_method_call failed" );
        }
    }
    else
    {
        AI_LOG_ERROR( "Invalid method: name %s, interface %s, path %s", method.name.c_str(), method.interface.c_str(), method.object.c_str());
    }

    AI_LOG_FN_EXIT();

    return replyGetter;
}

bool IpcService::invokeMethod(const Method& method, const VariantList& sendArgs, VariantList& replyArgs, int timeoutMs /*= -1*/)
{
    AI_LOG_FN_ENTRY();

    bool res = false;

    std::shared_ptr<IAsyncReplyGetter> replyGetter = invokeMethod(method, sendArgs, timeoutMs);
    if( replyGetter )
    {
        res = replyGetter->getReply(replyArgs);
    }
    else
    {
        AI_LOG_ERROR("Unable to create reply getter");
    }

    AI_LOG_FN_EXIT();

    return res;
}

bool IpcService::emitSignal( const Signal& signal, const VariantList& args )
{
    AI_LOG_FN_ENTRY();

    bool res = false;

    if ( validRemoteEntry(signal) )
    {
        DBusMessage *msg = dbus_message_new_signal( signal.object.c_str(), signal.interface.c_str(), signal.name.c_str() );
        if ( msg != NULL )
        {
            if( appendArgsToDbusMsg(msg, args) )
            {
                res = mDbusConnection->sendMessageNoReply(msg);
            }
            else
            {
                AI_LOG_ERROR("Unable to append arguments to dbus message");
            }

            dbus_message_unref(msg);
        }
        else
        {
            AI_LOG_ERROR( "Unable to create dbus message for new signal" );
        }
    }
    else
    {
        AI_LOG_ERROR( "Invalid signal: name %s, interface %s, path %s", signal.name.c_str(), signal.interface.c_str(), signal.object.c_str());
    }

    AI_LOG_FN_EXIT();

    return res;
}

std::string IpcService::registerMethodHandler(const Method& method, const MethodHandler& handler)
{
    AI_LOG_FN_ENTRY();

    std::string regId;

    if ( validRemoteEntry(method) )
    {
        if( method.service == mServiceName )
        {
            std::unique_lock<std::mutex> lock(mMutex);

            const std::string matchRule = getMatchRule(method);

            if ( mMethodHandlers.find(matchRule) == mMethodHandlers.end() )
            {
                // Add the object path for the dbus handler filter
                registerObjectPath(method.object);

                // Add the handler
                regId = matchRule;
                mMethodHandlers[matchRule] = std::pair<Method, MethodHandler>(method, handler);

                // We need to drop the lock when calling the dbus API as it may callback our handler
                // which in turn will try and take the lock (we could use recursive mutex, but seems
                // overkill in this situation)
                lock.unlock();

                // Finally try and add the rule, if fails then remove the handler
                if (!mDbusConnection->addMatch(matchRule.c_str()))
                {
                    AI_LOG_ERROR("failed to add match rule");

                    lock.lock();

                    // Failed - retake the lock and remove the handler and return an empty id
                    mMethodHandlers.erase(matchRule);
                    regId.clear();
                }

            }
            else
            {
                AI_LOG_ERROR( "Method handler already registered for this match rule %s", matchRule.c_str() );
            }
        }
        else
        {
            AI_LOG_ERROR( "Invalid service name %s", method.service.c_str() );
        }
    }
    else
    {
        AI_LOG_ERROR( "Invalid method: name %s, interface %s, path %s", method.name.c_str(), method.interface.c_str(), method.object.c_str() );
    }

    AI_LOG_FN_EXIT();

    return regId;
}

std::string IpcService::registerSignalHandler(const Signal& signal, const SignalHandler& handler)
{
    AI_LOG_FN_ENTRY();

    std::string regId;

    if ( validRemoteEntry(signal) )
    {
        std::string matchRule = getMatchRule(signal);

        if (mDbusConnection->addMatch(matchRule) == true)
        {
            std::lock_guard<std::mutex> lock(mMutex);

            // Add the object path for the dbus handler filter
            registerObjectPath(signal.object);

            // We do not use match rule as multiple handler can be registered for same signal
            regId = boost::lexical_cast<std::string>(mNextSignalHandlerRegId++);
            mSignalHandlers[regId] = std::pair<Signal, SignalHandler>(signal, handler);
        }
        else
        {
            AI_LOG_ERROR( "Failed to add signal match rule \"%s\"", matchRule.c_str() );
        }
    }
    else
    {
        AI_LOG_ERROR( "Invalid signal: name %s, interface %s, path %s", signal.name.c_str(), signal.interface.c_str(), signal.object.c_str() );
    }

    AI_LOG_FN_EXIT();

    return regId;
}

bool IpcService::unregisterHandler(const std::string& regId)
{
    AI_LOG_FN_ENTRY();

    bool res = true;

    std::lock_guard<std::mutex> lock(mMutex);

    std::string matchRule;
    std::string objectPath;

    auto iterMethodHandler = mMethodHandlers.find(regId);
    if( iterMethodHandler != mMethodHandlers.end() )
    {
        matchRule = getMatchRule(iterMethodHandler->second.first);
        objectPath = iterMethodHandler->second.first.object;
        mMethodHandlers.erase(iterMethodHandler);
    }
    else
    {
        auto iterSignalHandler = mSignalHandlers.find(regId);
        if( iterSignalHandler != mSignalHandlers.end() )
        {
            matchRule = getMatchRule(iterSignalHandler->second.first);
            objectPath = iterSignalHandler->second.first.object;
            mSignalHandlers.erase(iterSignalHandler);
        }
        else
        {
            AI_LOG_ERROR( "Unable to unregister: invalid registration Id %s", regId.c_str() );
            res = false;
        }
    }

    if ( !objectPath.empty() )
    {
        unregisterObjectPath(objectPath);
    }

    if ( !matchRule.empty() )
    {
        mDbusConnection->removeMatch(matchRule);
    }

    AI_LOG_FN_EXIT();

    return res;
}

DBusHandlerResult IpcService::handleDbusMessageCb( DBusMessage *message )
{
    AI_LOG_FN_ENTRY();

    DBusHandlerResult res = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

#if (AI_BUILD_TYPE == AI_DEBUG)
    if (mInMonitorMode)
    {
        res = handleDbusMonitorEvent(message);
    }
    else
#endif
    {
        res = handleDbusMessage(message);
    }

    AI_LOG_FN_EXIT();

    return res;
}

#if (AI_BUILD_TYPE == AI_DEBUG)
DBusHandlerResult IpcService::handleDbusMonitorEvent( DBusMessage *dbusMsg )
{
    AI_LOG_FN_ENTRY();

    const char *sender = dbus_message_get_sender(dbusMsg);
    const char *destination = dbus_message_get_destination(dbusMsg);
    const char *objectPath = nullptr;
    const char *interface = nullptr;
    const char *name = nullptr;

    std::lock_guard<std::mutex> lock(mMutex);
    if ( mMonitorCb )
    {
        EventType type;
        unsigned int serial;

        switch ( dbus_message_get_type(dbusMsg) )
        {
            case DBUS_MESSAGE_TYPE_METHOD_CALL:
                type = MethodCallEvent;
                serial = dbus_message_get_serial(dbusMsg),
                objectPath = dbus_message_get_path(dbusMsg),
                interface = dbus_message_get_interface(dbusMsg),
                name = dbus_message_get_member(dbusMsg);
                break;
            case DBUS_MESSAGE_TYPE_SIGNAL:
                type = SignalEvent;
                serial = dbus_message_get_serial(dbusMsg),
                objectPath = dbus_message_get_path(dbusMsg),
                interface = dbus_message_get_interface(dbusMsg),
                name = dbus_message_get_member(dbusMsg);
                break;
            case DBUS_MESSAGE_TYPE_METHOD_RETURN:
                type = MethodReturnEvent;
                serial = dbus_message_get_reply_serial(dbusMsg);
                break;
            case DBUS_MESSAGE_TYPE_ERROR:
                type = ErrorEvent;
                serial = dbus_message_get_reply_serial(dbusMsg);
                name = dbus_message_get_error_name(dbusMsg);
                break;
            default:
                AI_LOG_ERROR_EXIT("Unknown message type received");
                return DBUS_HANDLER_RESULT_HANDLED;
        }

        DbusMessageParser dbusMessageParser(dbusMsg);
        if ( !dbusMessageParser.parseMsg() )
        {
            AI_LOG_ERROR_EXIT("Failed to parse args for monitor event of type %d", type);
            return DBUS_HANDLER_RESULT_HANDLED;
        }

        mHandlerDispatcher.post( std::bind( mMonitorCb, type, serial,
                                            std::string(sender ? sender : ""),
                                            std::string(destination ? destination : ""),
                                            std::string(objectPath ? objectPath : ""),
                                            std::string(interface ? interface : ""),
                                            std::string(name ? name : ""),
                                            dbusMessageParser.getArgList() ) );
    }

    AI_LOG_FN_EXIT();

    return DBUS_HANDLER_RESULT_HANDLED;
}
#endif // if(AI_BUILD_TYPE == AI_DEBUG)

DBusHandlerResult IpcService::handleDbusMessage( DBusMessage *dbusMsg )
{
    AI_LOG_FN_ENTRY();

    DBusHandlerResult res = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    const char *objectPath = dbus_message_get_path(dbusMsg);
    const char *interface = dbus_message_get_interface(dbusMsg);
    const char *name = dbus_message_get_member(dbusMsg);

    bool isSignal = false;
    bool isMethod = false;

#if (AI_BUILD_TYPE == AI_DEBUG)
    if(objectPath && interface && name)
    {
        const char *sender = dbus_message_get_sender(dbusMsg);
        const char *destination = dbus_message_get_destination (dbusMsg);

        AI_LOG_DEBUG( "Received objectPath %s", objectPath );
        AI_LOG_DEBUG( "Received interface %s", interface );
        AI_LOG_DEBUG( "Received name %s", name );
        AI_LOG_DEBUG( "Received sender %s", sender );
        AI_LOG_DEBUG( "Received destination %s", destination );
    }
#endif

    if( objectPath && isRegisteredObjectPath(objectPath) )
    {
        if ( (objectPath != NULL) && (interface != NULL) && (name != NULL) )
        {
            if( dbus_message_is_method_call(dbusMsg, interface, name) )
            {
                isMethod = true;
                AI_LOG_DEBUG( "Method call received" );

            }
            else if( dbus_message_is_signal(dbusMsg, interface, name) )
            {
                isSignal = true;
                AI_LOG_DEBUG( "Signal received" );
            }
        }

        if ( isSignal || isMethod )
        {
            DbusMessageParser dbusMessageParser(dbusMsg);
            if ( dbusMessageParser.parseMsg() )
            {
                if ( isSignal )
                {
                    res = handleDbusSignal( Signal(objectPath, interface, name), dbusMessageParser.getArgList() );
                }
                else
                {
                    const char *sender = dbus_message_get_sender(dbusMsg);
                    if(isDbusMessageAllowed(sender, interface))
                    {
                        res = handleDbusMethodCall( Method(mServiceName, objectPath, interface, name), dbusMessageParser.getArgList(), dbusMsg );
                    }
                }
            }
            else
            {
                AI_LOG_ERROR( "Unable to parse arguments" );
            }
        }
    }

    AI_LOG_FN_EXIT();

    return res;
}

bool IpcService::isDbusMessageAllowed(const std::string& sender, const std::string& interface)
{
    AI_LOG_FN_ENTRY();

    bool res = true;

    // do the dbus entitlement check only, if:
    // - the IPCService has been created with a package manager parameter - indicating that the caches need to be created
    // - the ai configuration enables the entitlement check
    if( mDbusPackageEntitlements && mDbusEntitlementCheckNeeded)
    {
        AI_LOG_INFO("IpcService needs to do dbus capability check - received interface: %s", interface.c_str());
        // indicating a special IpcService, where Dbus capability check is needed

        if(!mDbusPackageEntitlements->isInterfaceWhiteListed(interface))
        {
            AI_LOG_INFO("%s interface is not white listed, checking the entitlements", interface.c_str());

            boost::optional<uid_t> userId = mDbusUserIdSenderIdCache->getUserId(sender);

            if(!userId)
            {
                // userId is not in the cache yet - need to get it...
                // this is expensive, that's why it will be cached
                uid_t uid = mDbusConnection->getUnixUser(sender);

#if (AI_BUILD_TYPE == AI_DEBUG)
            // if in debug mode, and if the root user sent the DBus message, don't check the entitlements
            if( uid == 0 )
            {
                AI_LOG_DEBUG("DBus message sent by root in debug build, not checking Dbus entitlements");
                return true;
            }
#endif

                mDbusUserIdSenderIdCache->addSenderIUserId(sender, uid);
                userId = uid;
            }

            if( !mDbusPackageEntitlements->isAllowed( *userId, mServiceName, interface ))
            {
                res = false;
            }
        }
    }

    AI_LOG_FN_EXIT();

    return res;
}

DBusHandlerResult IpcService::handleDbusSignal( const Signal& signal, const VariantList& argList )
{
    AI_LOG_FN_ENTRY();

    DBusHandlerResult res = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    std::lock_guard<std::mutex> lock(mMutex);

    for ( auto iter = mSignalHandlers.begin(); iter != mSignalHandlers.end(); ++iter )
    {
        if( iter->second.first == signal )
        {
            mHandlerDispatcher.post( std::bind(iter->second.second, argList ) );
            res = DBUS_HANDLER_RESULT_HANDLED; // continue as many can be interested in the same signal
        }
    }

    AI_LOG_FN_EXIT();

    return res;
}

DBusHandlerResult IpcService::handleDbusMethodCall( const Method& method, const VariantList& argList, DBusMessage *dbusMsg )
{
    AI_LOG_FN_ENTRY();

    DBusHandlerResult res = DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    std::lock_guard<std::mutex> lock(mMutex);

    auto iter = mMethodHandlers.find(getMatchRule(method));
    if( iter != mMethodHandlers.end() )
    {
        DBusMessage *replyMsg = dbus_message_new_method_return(dbusMsg);
        if ( replyMsg )
        {
            std::shared_ptr<AsyncReplySender> asyncReplySender = std::make_shared<AsyncReplySender>(mDbusConnection, dbusMsg, replyMsg, argList);
            mHandlerDispatcher.post(std::bind(iter->second.second, asyncReplySender));
        }

        res = DBUS_HANDLER_RESULT_HANDLED;
    }

    AI_LOG_FN_EXIT();

    return res;
}

void IpcService::flush()
{
    AI_LOG_FN_ENTRY();

    // This will ensure that any handlers queued on the dispatcher are processed
    // before returning
    mHandlerDispatcher.sync();

    AI_LOG_FN_EXIT();
}

bool IpcService::start()
{
    AI_LOG_FN_ENTRY();

    bool res = false;

    if ( !mRunning )
    {
        mRunning = true;
        mDbusConnection->registerMessageHandler(std::bind(&IpcService::handleDbusMessageCb, this, std::placeholders::_1));
        res = true;
    }
    else
    {
        AI_LOG_ERROR("IPC service already started: start() has not impact");
    }

    AI_LOG_FN_EXIT();

    return res;
}

bool IpcService::stop()
{
    AI_LOG_FN_ENTRY();

    bool res = false;

    if ( mRunning )
    {
        mRunning = false;
        mDbusConnection->registerMessageHandler(nullptr);
        mHandlerDispatcher.sync();
        res = true;
    }
    else
    {
        AI_LOG_INFO("IPC service not running - stop() has no impact");
    }

    AI_LOG_FN_EXIT();

    return res;
}

void IpcService::registerObjectPath(const std::string& path)
{
#if (AI_BUILD_TYPE == AI_DEBUG)
    if( mMutex.try_lock() )
    {
        AI_LOG_ERROR("registerObjectPath called without lock held");
        mMutex.unlock();
    }
#endif

    std::map<std::string,int>::iterator it = mObjectPaths.find(path);
    if( it == mObjectPaths.end() )
    {
        mObjectPaths[path] = 1;
    }
    else
    {
        it->second++;
    }
}

void IpcService::unregisterObjectPath(const std::string& path)
{
#if (AI_BUILD_TYPE == AI_DEBUG)
    if( mMutex.try_lock() )
    {
        AI_LOG_ERROR("unregisterObjectPath called without lock held");
        mMutex.unlock();
    }
#endif

    std::map<std::string,int>::iterator it = mObjectPaths.find(path);
    if( it == mObjectPaths.end() )
    {
         AI_LOG_ERROR("object path '%s' not registered", path.c_str());
    }
    else if( --(it->second) == 0 )
    {
        mObjectPaths.erase(it);
    }
}

bool IpcService::isRegisteredObjectPath(const std::string& path)
{
    std::lock_guard<std::mutex> lock(mMutex);
    return ( mObjectPaths.find(path) != mObjectPaths.end() );
}

void IpcService::unregisterHandlers()
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> lock(mMutex);

    std::string matchRule;

    for ( auto iterMH = mMethodHandlers.begin(); iterMH != mMethodHandlers.end(); ++iterMH )
    {
        matchRule = getMatchRule(iterMH->second.first);

        mDbusConnection->removeMatch(matchRule);
    }

    mMethodHandlers.clear();

    for ( auto iterSH = mSignalHandlers.begin(); iterSH != mSignalHandlers.end(); ++iterSH )
    {
        matchRule = getMatchRule(iterSH->second.first);

        mDbusConnection->removeMatch(matchRule);
    }

    mSignalHandlers.clear();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Enables monitor mode on the IPC service, this will effectively
 *  disable all registered method and signal handlers
 *
 *  This function is for debugging only, it can be used to monitor the entire
 *  bus and the interactions between clients.
 *
 *  For production builds this function always returns false.
 *
 *  @note As of dbus version 1.9.10 a new API was added to the daemon;
 *  org.freedesktop.DBus.Monitoring.BecomeMonitor, this is more convenient than
 *  using the magic eavesdrop=true match pattern.  However currently we're still
 *  on an old dbus version that doesn't have that support.
 *
 */
bool IpcService::enableMonitor(const std::set<std::string>& matchRules, const MonitorHandler& handler)
{
#if (AI_BUILD_TYPE != AI_DEBUG)

    return false;

#else // if (AI_BUILD_TYPE == AI_DEBUG)

    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> lock(mMutex);

    // If already in monitor mode then remove any previous monitor match rules first
    if ( mInMonitorMode )
    {
        for (const std::string& rule : mMonitorMatchRules)
        {
            mDbusConnection->removeMatch(rule);
        }
    }

    // Clear all the previous match rules
    mMonitorMatchRules.clear();

    // If no match rules were supplied then add the default capture all rule
    if ( matchRules.empty() )
    {
        mMonitorMatchRules.emplace( "eavesdrop=true" );
    }
    else
    {
        const std::string rulePrefix( "eavesdrop=true," );

        for (const std::string& rule : matchRules)
        {
            mMonitorMatchRules.emplace( rulePrefix + rule );
        }
    }

    // Set the handler and enable monitor mode now before setting the eavesdrop rules
    mMonitorCb = handler;
    mInMonitorMode = true;

    // Add all the match rules
    for (const std::string& rule : mMonitorMatchRules)
    {
        mDbusConnection->addMatch(rule);
    }

    AI_LOG_FN_EXIT();
    return true;

#endif // if (AI_BUILD_TYPE == AI_DEBUG)
}

// -----------------------------------------------------------------------------
/**
 *  @brief Disables monitor mode and restores normal behaviour
 *
 *  It's recommended that @a flush() is called after this function to ensure
 *  that the monitor callback will no longer be called.
 *
 *
 */
bool IpcService::disableMonitor()
{
#if (AI_BUILD_TYPE != AI_DEBUG)

    return false;

#else // if (AI_BUILD_TYPE == AI_DEBUG)

    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> lock(mMutex);

    // If monitor mode wasn't enabled of the match rules were empty then we were
    // never in monitor mode in the first place
    if ( !mInMonitorMode || mMonitorMatchRules.empty() )
    {
        AI_LOG_WARN("Not in monitor mode");
        AI_LOG_FN_EXIT();
        return false;
    }

    // Remove all the monitor match rules
    for (const std::string& rule : mMonitorMatchRules)
    {
        mDbusConnection->removeMatch(rule);
    }

    // Disable the monitor mode flag and clear the callback
    mInMonitorMode = false;
    mMonitorCb = nullptr;

    AI_LOG_FN_EXIT();
    return true;

#endif // if (AI_BUILD_TYPE == AI_DEBUG)
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks if the named service is available on the bus
 *
 *  This method is expected to be used at start to determine if the daemons
 *  are up and running.
 *
 *  The method doesn't wait for the service to arrive, that would be nice to
 *  have and could be done by looking for the signals dbus send when a new
 *  client arrives.  We could add that in the future rather than having this
 *  polling interface.
 *
 *  @param[in]  serviceName     The name of the service to check for.
 *
 *  @return false if an error occurried or the serviceName doesn't exist,
 *  otherwise true.
 */
bool IpcService::isServiceAvailable(const std::string& serviceName) const
{
    AI_LOG_FN_ENTRY();

#if (AI_BUILD_TYPE == AI_DEBUG)
    if ( !mRunning )
    {
        AI_LOG_WARN("Trying to check the serviceName without IpcService event loop running");
    }
#endif

    return mDbusConnection->nameHasOwner(serviceName);
}

