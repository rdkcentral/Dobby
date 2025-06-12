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
 * File:   DobbyIpcBus.h
 *
 */
#include "DobbyIpcBus.h"

#include <Logging.h>
#include <IpcFactory.h>

#include <dbus/dbus.h>

#include <stdio.h>
#include <cstring>



DobbyIpcBus::DobbyIpcBus(const std::string& dbusAddress,
                         const std::shared_ptr<AI_IPC::IIpcService>& ipcService)
    : mService(ipcService)
    , mDbusAddress(dbusAddress)
    , mDbusSocketPath(socketPathFromAddress(dbusAddress))
    , mHandlerId(1)
{
    // spawn the thread used for calling the service change callbacks
    mServiceChangeThread = std::thread(&DobbyIpcBus::serviceChangeThread, this);

    // install a signal handler to watch for services arriving / leaving the bus
    registerServiceWatcher();
}

DobbyIpcBus::DobbyIpcBus()
    : mHandlerId(1)
{
    // spawn the thread used for calling the service change callbacks
    mServiceChangeThread = std::thread(&DobbyIpcBus::serviceChangeThread, this);
}

DobbyIpcBus::~DobbyIpcBus()
{
    // disconnect the dbus service
    disconnect();

    // set the terminate flag and wake the thread
    if (mServiceChangeThread.joinable())
    {
        std::unique_lock<std::mutex> locker(mLock);
        mServiceChangeQueue.emplace_back(ServiceChangeEvent::Terminate);
        locker.unlock();

        mServiceChangeCond.notify_all();
        mServiceChangeThread.join();
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Simply returns the dbus address if we have one
 *
 *  If not currently connected to a service this will return an empty string.
 *
 */
const std::string& DobbyIpcBus::address() const
{
    return mDbusAddress;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns just the socket path of the dbus address
 *
 *  If not currently connected to a service this will return an empty string.
 *
 */
const std::string& DobbyIpcBus::socketPath() const
{
    return mDbusSocketPath;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility function to extract the socket path from the dbus address
 *  string.
 *
 *  This uses the low level dbus library API to parse the address and extract
 *  the fields.  If the address supplied is not a unix socket then an empty
 *  string is returned.
 *
 *  @param[in]  address     The dbus address trying to parse
 *
 *  @return on success the path to the dbus socket, on failure an empty string.
 */
std::string DobbyIpcBus::socketPathFromAddress(const std::string& address)
{
    AI_LOG_FN_ENTRY();

    if (address.empty())
    {
        return std::string();
    }

    std::string socketPath;

    // use the low level dbus functions to extract just the socket path from
    // the address
    DBusError err;
    dbus_error_init(&err);

    DBusAddressEntry** entries = nullptr;
    int nEntries = 0;

    dbus_bool_t result = dbus_parse_address(address.c_str(), &entries, &nEntries, &err);
    if (!result || !entries || dbus_error_is_set(&err))
    {
        AI_LOG_ERROR_EXIT("failed to parse address ('%s')", err.message);
        dbus_error_free(&err);
        dbus_address_entries_free(entries);
        return socketPath;
    }

    // process the results
    for (int i = 0; i < nEntries; i++)
    {
        const char* method = dbus_address_entry_get_method(entries[i]);
        if (method && (strcmp(method, "unix") == 0))
        {
            const char* path = dbus_address_entry_get_value(entries[i], "path");
            if (path)
            {
                socketPath = path;
                break;
            }
        }
    }

    // free the memory allocated for the results
    dbus_address_entries_free(entries);

    if (socketPath.empty())
    {
        AI_LOG_ERROR("failed to find unix socket path in address");
    }

    AI_LOG_FN_EXIT();
    return socketPath;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Tries to connect to the bus at the given address.
 *
 *  This method will close any existing connection first before trying to
 *  to connect to the new address.  If the method fails to connect to the
 *  new bus the old connection is not restored, the bus will be left in
 *  the disconnected state.
 *
 *  @param[in]  dbusAddress     The dbus address to connect to.
 *
 *  @return true if managed to connect to the bus, otherwise false.
 */
bool DobbyIpcBus::connect(const std::string& dbusAddress)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // tear down the old service
    disconnectNoLock();

    // create a pseudo unique name for our service (this is only needed
    // because we may already have a connection to the bus in question).
    char serviceName[64];
    sprintf(serviceName, "org.rdk.dobby.pid%d", getpid());

    // create IPCServices that attach to the dbus daemon, this throws an
    // exception if it can't connect, nb the default timeout is set to
    // 5 seconds rather the default 30 seconds
    try
    {
        mService = AI_IPC::createIpcService(dbusAddress, serviceName, 5000);
    }
    catch (const std::exception& e)
    {
        AI_LOG_ERROR_EXIT("failed to create ipc service, due to '%s'",
                          e.what());
        return false;
    }

    // install a signal handler to watch for services arriving / leaving the bus
    registerServiceWatcher();

    // start the ipc service thread, if it fails we destroy the service object
    // and give up ... hope it doesn't fail
    if (!mService->start())
    {
        mService.reset();

        AI_LOG_ERROR_EXIT("failed to start the ipc service");
        return false;
    }

    // since we've now (re)connected, check if we have any signals we need to
    // (re)install on the service
    std::map<int, SignalHandler>::iterator it = mSignalHandlers.begin();
    for (; it != mSignalHandlers.end(); ++it)
    {
        const AI_IPC::Signal& signal = it->second.signal;
        const AI_IPC::SignalHandler& handler = it->second.handler;

        it->second.regId = mService->registerSignalHandler(signal, handler);
        if (it->second.regId.empty())
        {
            AI_LOG_ERROR("failed to register signal handler");
        }
    }

    // also check if we need to signal that any services are available now
    for (const std::pair<const int, ServiceHandler>& handler : mServiceHandlers)
    {
        const std::string& service = handler.second.name;
        //const ServiceHandlerFn& callback = handler.second.handler;

        if (mService->isServiceAvailable(service))
        {
            // push a service added event to the queue
            std::lock_guard<std::mutex> queueLocker(mServiceChangeLock);
            mServiceChangeQueue.emplace_back(ServiceChangeEvent::ServiceAdded,
                                             service);
            mServiceChangeCond.notify_all();
        }
    }

    // last step is to store the address
    mDbusAddress = dbusAddress;
    mDbusSocketPath = socketPathFromAddress(mDbusAddress);

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Simply disconnects from the bus.
 *
 *  If there were any service notifiers installed they will each get
 *  a 'service left' callback (provided the bus was actually connected).
 *
 */
void DobbyIpcBus::disconnect()
{
    std::lock_guard<std::mutex> locker(mLock);
    disconnectNoLock();
}

// -------------------------------------------------------------------------
/**
 *  @brief Disconnects the service from the bus.
 *
 *  This will call any service notifiers to tell them that their interested
 *  service(s) has left the bus.  Obviously this may not actually be true,
 *  but since we're closing our connection to the bus it might as well be
 *  because there is no way to now talk to those services.
 *
 *  It then flushes out all messages and removes the signal notifier.
 *
 */
void DobbyIpcBus::disconnectNoLock()
{
    AI_LOG_FN_ENTRY();

    if (mService)
    {
        // unregister the signal handler and then flush all the messages out
        if (!mServiceSignal.empty())
        {
            mService->unregisterHandler(mServiceSignal);
        }

        // unregister any other signal handlers
        for (std::pair<const int, SignalHandler>& signal : mSignalHandlers)
        {
            if (!signal.second.regId.empty())
            {
                mService->unregisterHandler(signal.second.regId);
            }

            signal.second.regId.clear();
        }

        // flush and stop the IPC service
        mService->flush();
        mService->stop();

        // push service disappeared events onto the event thread queue, because
        // obviously the bus is disappeared so the services are no longer
        // available
        {
            std::lock_guard<std::mutex> locker(mServiceChangeLock);

            for (const std::pair<const int, ServiceHandler> &handler : mServiceHandlers)
            {
                mServiceChangeQueue.emplace_back(ServiceChangeEvent::ServiceRemoved,
                                                 handler.second.name);
            }

            mServiceChangeCond.notify_all();
        }

        // close the service connection
        mService.reset();

        // clear the dbus address
        mDbusAddress.clear();
        mDbusSocketPath.clear();
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Install a signal handler to detect services arriving / leaving
 *  the bus.
 *
 *  Installs a signal listener for the 'org.freedesktop.DBus.NameOwnerChanged'
 *  signal which is used to tell when services arrive and leave the bus, we
 *  use it to implement the DobbyUtils::ipcServiceNotify() method
 *
 *  The method updates the @a mNotifierSignal internal string to hold the
 *  the registered handler.  It is assumed that the handler is not already
 *  installed.
 *
 */
void DobbyIpcBus::registerServiceWatcher()
{
    static const AI_IPC::Signal signal("/org/freedesktop/DBus",
                                       "org.freedesktop.DBus",
                                       "NameOwnerChanged");

    AI_IPC::SignalHandler handler = std::bind(&DobbyIpcBus::serviceNameChanged,
                                              this, std::placeholders::_1);

    mServiceSignal = mService->registerSignalHandler(signal, handler);
    if (mServiceSignal.empty())
    {
        AI_LOG_ERROR("failed to register signal handler for '%s.%s'",
                     signal.interface.c_str(), signal.name.c_str());
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Invokes the ipc method
 *
 *  This is a pure wrapper around the IpcService::invokeMethod function.
 *
 *  @param[in]  method      The method to call.
 *  @param[in]  args        The method args
 *  @param[in]  timeoutMs   Timeout in milliseconds, -1 for default (5 seconds)
 *
 *  @return A result to wait on.
 */
std::shared_ptr<AI_IPC::IAsyncReplyGetter> DobbyIpcBus::invokeMethod(const AI_IPC::Method& method,
                                                                     const AI_IPC::VariantList& args,
                                                                     int timeoutMs) const
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!mService)
        return nullptr;

    return mService->invokeMethod(method, args, timeoutMs);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Invokes the ipc method
 *
 *  This is a pure wrapper around the IpcService::invokeMethod function.
 *
 *  @param[in]  method      The method to call.
 *  @param[in]  args        The method args
 *  @param[out] replyArgs   The reply.
 *
 *  @return true if successful, otherwise false.
 */
bool DobbyIpcBus::invokeMethod(const AI_IPC::Method& method,
                               const AI_IPC::VariantList& args,
                               AI_IPC::VariantList& replyArgs) const
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!mService)
        return false;

    return mService->invokeMethod(method, args, replyArgs);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends out a signal over dbus.
 *
 *  This is a pure wrapper around the IpcService::emitSignal function.
 *
 *  @param[in]  signal      The signal details.
 *  @param[in]  args        The signal args.
 *
 *  @return true if successful, otherwise false.
 */
bool DobbyIpcBus::emitSignal(const AI_IPC::Signal& signal,
                             const AI_IPC::VariantList& args) const
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!mService)
        return false;

    return mService->emitSignal(signal, args);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Queries if the given service is available on the bus.
 *
 *  This is a pure wrapper around the IpcService::serviceAvailable function.
 *
 *  @param[in]  serviceName The service to query.
 *
 *  @return true if the service is available, otherwise false.
 */
bool DobbyIpcBus::serviceAvailable(const std::string& serviceName) const
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!mService)
        return false;

    return mService->isServiceAvailable(serviceName);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Registers a callback function that will be called when the given
 *  service is added or removed from the bus.
 *
 *  This in turn is useful for hooks to manage situations where the daemon
 *  they are talking to has crashed / restarted.
 *
 *  Case in point is the Jumper hook, it wants to know if the daemon has
 *  crashed so it doesn't block container startup by trying to talk to a
 *  nonexisting daemon. And likewise it wants to know when it's arrived back
 *  so it can re-create any state stored in the daemon.
 *
 *  To remove the handler call @a ipcUnregisterHandler with the handler
 *  id returned by this function.
 *
 *  @param[in]  serviceName     The name of the service to look out for.
 *  @param[in]  handlerFunc     Callback function called when the service is
 *                              added or removed.  If added the argument
 *                              supplied will be true, if removed it will be
 *                              false.
 *
 *  @return if the notifier is successifully added then a positive handler
 *  id will be returned, otherwise -1
 */
int DobbyIpcBus::registerServiceHandler(const std::string& serviceName,
                                        const ServiceHandlerFn& handlerFunc)
{
    std::lock_guard<std::mutex> locker(mLock);

    // all we need to do is store the service handler in our map
    int id = mHandlerId++;
    mServiceHandlers.emplace(id, ServiceHandler(serviceName, handlerFunc));

    return id;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Registers a callback function that will be called when the given
 *  signal is received on the bus.
 *
 *  This is a pure wrapper around the IpcService.registerSignalHandler
 *  function.
 *
 *  @param[in]  signal          The signal details to watch for.
 *  @param[in]  handlerFunc     Callback function called when the signal is
 *                              received.
 *
 *  @return if the handler is successifully added then a positive handler
 *  id will be returned, otherwise -1
 */
int DobbyIpcBus::registerSignalHandler(const AI_IPC::Signal& signal,
                                       const AI_IPC::SignalHandler& handlerFunc)
{
    std::lock_guard<std::mutex> locker(mLock);

    std::string signalId;

    // if we have a valid service try and register the signal handler
    if (mService)
    {
        signalId = mService->registerSignalHandler(signal, handlerFunc);
        if (signalId.empty())
        {
            AI_LOG_ERROR("failed to register signal");
        }
    }

    // store the signal handler in our internal map
    int id = mHandlerId++;
    mSignalHandlers.emplace(id, SignalHandler(signalId, signal, handlerFunc));

    return id;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Unregisters a signal or service handler.
 *
 *
 *
 *  @param[in]  handlerId       The handler id returned by either the
 *                              registerSignalHandler or registerServiceHandler
 *                              methods.
 */
void DobbyIpcBus::unregisterHandler(int handlerId)
{
    std::unique_lock<std::mutex> locker(mLock);

    // try and find the handler in the signals
    std::map<int, SignalHandler>::iterator it = mSignalHandlers.find(handlerId);
    if (it != mSignalHandlers.end())
    {
        // if we have a running ipc service then unregister the signal
        if (mService && !it->second.regId.empty())
        {
            mService->unregisterHandler(it->second.regId);
        }

        // now just remove the entry from our map
        mSignalHandlers.erase(it);

        // and we're done
        return;
    }

    // try and find the handler in the service watcher map
    std::map<int, ServiceHandler>::iterator jt = mServiceHandlers.find(handlerId);
    if (jt != mServiceHandlers.end())
    {
        // just need to remove it from our local map
        mServiceHandlers.erase(jt);

        // and we're done
        return;
    }

    // if we've arrived here it means the handlerId was bogus
    AI_LOG_ERROR("invalid handler id %d", handlerId);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback function called when dbus has informed us that a name on
 *  the bus has changed.
 *
 *  @see https://dbus.freedesktop.org/doc/dbus-specification.html#bus-messages-name-owner-changed
 *
 *  We use this signal to notify any listeners (typically hooks) that a
 *  service has arrived or left the bus.  This in turn is useful for hooks
 *  to manage situations where the daemon they are talking to has crashed /
 *  restarted.
 *
 *  Case in point is the Jumper hook, it wants to know if the daemon has
 *  crashed so it doesn't block container startup trying to talk to
 *  non-existing daemon. And likewise it wants to know when it's arrived back
 *  so it can re-create any state stored in the daemon.
 *
 *  @param[in]  args        The args received
 *
 */
void DobbyIpcBus::serviceNameChanged(const AI_IPC::VariantList& args)
{
    // we're expecting 3 args all strings;
    std::string name;
    std::string oldOwner;
    std::string newOwner;

    if (!AI_IPC::parseVariantList
            <std::string, std::string, std::string>
            (args, &name, &oldOwner, &newOwner))
    {
        AI_LOG_ERROR("failed to parse 'NameOwnerChanged' signal");
        return;
    }

    // post an event to the service change thread
    std::lock_guard<std::mutex> locker(mServiceChangeLock);

    if (newOwner.empty())
    {
        // if new owner is empty it means the service has left the bus
        AI_LOG_DEBUG("'%s' service has left the bus", name.c_str());
        mServiceChangeQueue.emplace_back(ServiceChangeEvent::ServiceRemoved, name);
    }
    else
    {
        // if the new owner is not empty it means the service has
        // joined the bus
        AI_LOG_DEBUG("'%s' service has arrived on the bus", name.c_str());
        mServiceChangeQueue.emplace_back(ServiceChangeEvent::ServiceAdded, name);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Thread function that receives notifications on service changes and
 *  then calls the install handler.
 *
 *  We use a separate thread to notify of signal changes because we don't want
 *  to block the IpcService thread for long periods of time while plugins
 *  setup / teardown their IPC code.
 *
 *
 */
void DobbyIpcBus::serviceChangeThread()
{
    pthread_setname_np(pthread_self(), "AI_DBUS_SERVICE");

    AI_LOG_INFO("entered Ipc service change thread");

    std::unique_lock<std::mutex> locker(mServiceChangeLock);

    bool terminate = false;
    while (!terminate)
    {
        // wait for an event
        while (mServiceChangeQueue.empty())
        {
            mServiceChangeCond.wait(locker);
        }

        // process all events
        while (!mServiceChangeQueue.empty())
        {
            // take the first item off the queue
            const ServiceChangeEvent event = mServiceChangeQueue.front();
            mServiceChangeQueue.pop_front();

            // drop the lock before calling any callbacks
            locker.unlock();

            if (event.type == ServiceChangeEvent::Terminate)
            {
                // terminate event so just set the flag so we terminate the
                // thread when the queue is empty
                terminate = true;
            }
            else
            {
                // need to hold the lock before searching
                std::lock_guard<std::mutex> handlerLock(mLock);

                // check if we have any listener interested in this service
                for (const std::pair<const int, ServiceHandler>& handler : mServiceHandlers)
                {
                    if (event.serviceName == handler.second.name)
                    {
                        const ServiceHandlerFn& callback = handler.second.handler;
                        if (callback)
                            callback(event.type == ServiceChangeEvent::ServiceAdded);
                    }
                }
            }

            // re-take the lock and check for any more events
            locker.lock();
        }
    }

    AI_LOG_INFO("exiting Ipc service change thread");
}


