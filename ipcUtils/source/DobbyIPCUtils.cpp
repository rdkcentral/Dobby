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
 * File:   DobbyIPCUtils.cpp
 *
 * Copyright (C) BSKYB 2016+
 */
#include "DobbyIPCUtils.h"
#include "DobbyIpcBus.h"

#include <Logging.h>
#include <sstream>
#include <stdlib.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

DobbyIPCUtils::DobbyIPCUtils(const std::string& systemDbusAddress,
                       const std::shared_ptr<AI_IPC::IIpcService>& systemIpcService)
{
    AI_LOG_FN_ENTRY();

    // add the system bus ... this is the one constant
    mIpcBuses[IDobbyIPCUtils::BusType::SystemBus] =
        std::make_shared<DobbyIpcBus>(systemDbusAddress, systemIpcService);

    // add the AI public and private bus objects, since we don't have addresses
    // for these buses yet they are created in the disconnected state
    mIpcBuses[IDobbyIPCUtils::BusType::AIPublicBus] = std::make_shared<DobbyIpcBus>();
    mIpcBuses[IDobbyIPCUtils::BusType::AIPrivateBus] = std::make_shared<DobbyIpcBus>();

    AI_LOG_FN_EXIT();
}

DobbyIPCUtils::~DobbyIPCUtils()
{
    AI_LOG_FN_ENTRY();

    mIpcBuses.clear();

    AI_LOG_FN_EXIT();
}


// -----------------------------------------------------------------------------
/**
 *  @brief Utility function to simply return the bus object associated with
 *  the given bus id.
 *
 *  Note no need for locking in this method as the bus objects should have been
 *  created in the construction and only deleted in the destructor.  The only
 *  thing we need to check is if the @a bus param is valid.
 *
 *  @param[in]  bus     The bus to get.
 *
 *  @return on success a shared_ptr to the bus object, on failure a nullptr.
 */
std::shared_ptr<DobbyIpcBus> DobbyIPCUtils::getIpcBus(const IDobbyIPCUtils::BusType& bus) const
{
    std::map<IDobbyIPCUtils::BusType, std::shared_ptr<DobbyIpcBus>>::const_iterator it = mIpcBuses.find(bus);
    if (it == mIpcBuses.end())
        return nullptr;

    return it->second;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the dbus address for one of the AI dbus-daemons
 *
 *  The address is expected to be of the form
 *      'unix:path=<path_to_socket>'
 *
 *  @param[in]  privateBus      true if refers to the private bus.
 *  @param[in]  address         The address of the bus.
 *
 *  @return true if the address was validated by opening a connection to the
 *  bus, otherwise false.
 */
bool DobbyIPCUtils::setAIDbusAddress(bool privateBus, const std::string& address)
{
    AI_LOG_FN_ENTRY();

    // determine the bus type
    IDobbyIPCUtils::BusType bus = privateBus ? IDobbyIPCUtils::BusType::AIPrivateBus :
                                            IDobbyIPCUtils::BusType::AIPublicBus;

    // get a reference to the bus
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
    {
        AI_LOG_ERROR_EXIT("odd, missing reference to bus");
        return false;
    }

    // disconnect from the old bus (a no-op if not already connected)
    ipcBus->disconnect();

    // connect to the new address
    if (!ipcBus->connect(address))
    {
        AI_LOG_ERROR_EXIT("failed to connect to dbus @ '%s'",
                          address.c_str());
        return false;
    }

    AI_LOG_FN_EXIT();
    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns complete address to the dbus daemon
 *
 *  @param[in]  bus     The bus to get the socket path for.
 *
 *  @return the path to the socket, or an empty string if no socket is available.
 */
std::string DobbyIPCUtils::ipcDbusAddress(const IDobbyIPCUtils::BusType& bus) const
{
    // get a reference to the bus
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
    {
        AI_LOG_ERROR("odd, missing reference to bus");
        return std::string();
    }

    // get the dbus address, it is assumed to be prefixed with 'unix:path='
    return ipcBus->address();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns just the path to the socket for the dbus daemon
 *
 *  @param[in]  bus     The bus to get the socket path for.
 *
 *  @return the path to the socket, or an empty string if no socket is available.
 */
std::string DobbyIPCUtils::ipcDbusSocketPath(const IDobbyIPCUtils::BusType& bus) const
{
    // get a reference to the bus
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
    {
        AI_LOG_ERROR("odd, missing reference to bus");
        return std::string();
    }

    // get the dbus address, it is assumed to be prefixed with 'unix:path='
    return ipcBus->socketPath();
}

// -------------------------------------------------------------------------
/**
 *  @brief Registers a callback function that will be called when the given
 *  signal is received on the bus.
 *
 *  This is a pure wrapper around the IpcService.registerSignalHandler
 *  function.
 *
 *  @param[in]  bus             The bus to watch for the signal on.
 *  @param[in]  signal          The signal details to watch for.
 *  @param[in]  handlerFunc     Callback function called when the signal is
 *                              received.
 *
 *  @return if the handler is successifully added then a positive handler
 *  id will be returned, otherwise -1
 */
int DobbyIPCUtils::ipcRegisterSignalHandler(const IDobbyIPCUtils::BusType& bus,
                                         const AI_IPC::Signal& signal,
                                         const AI_IPC::SignalHandler& handlerFunc)
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return -1;

    return ipcBus->registerSignalHandler(signal, handlerFunc);
}

// -------------------------------------------------------------------------
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
 *  @param[in]  bus             The bus to watch the service on.
 *  @param[in]  serviceName     The name of the service to look out for.
 *  @param[in]  handlerFunc     Callback function called when the service is
 *                              added or removed.  If added the argument
 *                              supplied will be true, if removed it will be
 *                              false.
 *
 *  @return if the notifier is successifully added then a positive handler
 *  id will be returned, otherwise -1
 */
int DobbyIPCUtils::ipcRegisterServiceHandler(const IDobbyIPCUtils::BusType& bus,
                                          const std::string& serviceName,
                                          const std::function<void(bool)>& handlerFunc)
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return -1;

    return ipcBus->registerServiceHandler(serviceName, handlerFunc);
}

// -------------------------------------------------------------------------
/**
 *  @brief Unregisters either a service or signal handler.
 *
 *
 *
 *  @param[in]  bus         The bus to remove the handler for.
 *  @param[in]  handlerId   The integer handler id returned by the register
 *                          function.
 *
 */
void DobbyIPCUtils::ipcUnregisterHandler(const IDobbyIPCUtils::BusType& bus,
                                      int handlerId)
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return;

    ipcBus->unregisterHandler(handlerId);
}

// -------------------------------------------------------------------------
/**
 *  @brief Invokes the ipc method
 *
 *  This is a pure wrapper around the IpcService::invokeMethod function.
 *
 *  @param[in]  bus         The bus call the method on.
 *  @param[in]  method      The method to call.
 *  @param[in]  args        The method args
 *  @param[in]  timeoutMs   Timeout in milliseconds, -1 for default (5 seconds)
 *
 *  @return shared pointer pointing to a reply getter to receive reply
 *  asynchronously, or nullptr on failure.
 */
std::shared_ptr<AI_IPC::IAsyncReplyGetter> DobbyIPCUtils::ipcInvokeMethod(const IDobbyIPCUtils::BusType& bus,
                                                                       const AI_IPC::Method& method,
                                                                       const AI_IPC::VariantList& args,
                                                                       int timeoutMs /*= -1*/) const
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return nullptr;

    return ipcBus->invokeMethod(method, args, timeoutMs);
}

// -------------------------------------------------------------------------
/**
 *  @brief Invokes the ipc method
 *
 *  This is a pure wrapper around the IpcService::invokeMethod function.
 *
 *  @param[in]  bus         The bus call the method on.
 *  @param[in]  method      The method to call.
 *  @param[in]  args        The method args
 *  @param[out] replyArgs   The reply.
 *
 *  @return true if successful, otherwise false.
 */
bool DobbyIPCUtils::ipcInvokeMethod(const IDobbyIPCUtils::BusType& bus,
                                 const AI_IPC::Method& method,
                                 const AI_IPC::VariantList& args,
                                 AI_IPC::VariantList& replyArgs) const
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return false;

    return ipcBus->invokeMethod(method, args, replyArgs);
}

// -------------------------------------------------------------------------
/**
 *  @brief Sends out a signal over dbus.
 *
 *  This is a pure wrapper around the IpcService::emitSignal function.
 *
 *  @param[in]  bus         The bus to emit the signal on.
 *  @param[in]  signal      The signal details.
 *  @param[in]  args        The signal args.
 *
 *  @return true if successful, otherwise false.
 */
bool DobbyIPCUtils::ipcEmitSignal(const IDobbyIPCUtils::BusType& bus,
                               const AI_IPC::Signal& signal,
                               const AI_IPC::VariantList& args) const
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return false;

    return ipcBus->emitSignal(signal, args);
}

// -------------------------------------------------------------------------
/**
 *  @brief Queries if the given service is available on the bus.
 *
 *  This is a pure wrapper around the IpcService::serviceAvailable function.
 *
 *  @param[in]  serviceName The service to query.
 *
 *  @return true if the service is available, otherise false.
 */
bool DobbyIPCUtils::ipcServiceAvailable(const BusType& bus,
                                     const std::string& serviceName) const
{
    std::shared_ptr<DobbyIpcBus> ipcBus = getIpcBus(bus);
    if (!ipcBus)
        return false;

    return ipcBus->serviceAvailable(serviceName);
}