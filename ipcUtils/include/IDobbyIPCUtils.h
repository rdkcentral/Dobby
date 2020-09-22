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
 * File:   IDobbyIPCUtils.h
 *
 */
#ifndef IDOBBYIPCUTILS_H
#define IDOBBYIPCUTILS_H

#include <IIpcService.h>

#include <cstdint>
#include <string>
#include <list>
#include <functional>

#include <sys/types.h>
#include <sys/sysmacros.h>

// -----------------------------------------------------------------------------
/**
 *  @class IDobbyUtils_v1
 *  @brief Interface that exports some utilities that plugins may find useful
 *
 *  As it's name implies this is just a collection of standalone utility
 *  functions that wrap up some of the common things that plugins do.
 *
 *
 */
class IDobbyIPCUtils
{
public:
    virtual ~IDobbyIPCUtils() = default;

public:
    // -------------------------------------------------------------------------
    /**
     *  @enum BusType
     *  @brief The type of dbus to call methods on / emit signals
     *
     *
     */
    enum class BusType
    {
        NoneBus,
        SystemBus,
        AIPrivateBus,
        AIPublicBus
    };

    // -------------------------------------------------------------------------
    /**
     *  @brief Wrappers around the IPC services in the Dobby daemon
     *
     *  We provide these as wrappers so that hooks don't have to spin up their
     *  own connections to a particular bus, instead they can use the service
     *  threads already created inside the Dobby daemon.
     *
     *  TODO: flesh out a bit more
     */
    virtual std::shared_ptr<AI_IPC::IAsyncReplyGetter> ipcInvokeMethod(const BusType &bus,
                                                                       const AI_IPC::Method &method,
                                                                       const AI_IPC::VariantList &args,
                                                                       int timeoutMs = -1) const = 0;

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
    virtual bool ipcInvokeMethod(const BusType &bus,
                                 const AI_IPC::Method &method,
                                 const AI_IPC::VariantList &args,
                                 AI_IPC::VariantList &replyArgs) const = 0;

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
    virtual bool ipcEmitSignal(const BusType &bus,
                               const AI_IPC::Signal &signal,
                               const AI_IPC::VariantList &args) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Queries if the given service is available on the bus.
     *
     *  This is a pure wrapper around the IpcService::serviceAvailable function.
     *
     *  @param[in]  bus         The bus to check.
     *  @param[in]  serviceName The service to query.
     *
     *  @return true if the service is available, otherwise false.
     */
    virtual bool ipcServiceAvailable(const BusType &bus,
                                     const std::string &serviceName) const = 0;

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
    virtual int ipcRegisterServiceHandler(const BusType &bus,
                                          const std::string &serviceName,
                                          const std::function<void(bool)> &handlerFunc) = 0;

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
    virtual int ipcRegisterSignalHandler(const BusType &bus,
                                         const AI_IPC::Signal &signal,
                                         const AI_IPC::SignalHandler &handlerFunc) = 0;

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
    virtual void ipcUnregisterHandler(const BusType &bus, int handlerId) = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns complete address to the dbus daemon
     *
     *  @param[in]  bus     The bus to get the socket path for.
     *
     *  @return the path to the socket, or an empty string if no socket is
     *  available.
     */
    virtual std::string ipcDbusAddress(const BusType &bus) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns just the path to the socket for the dbus daemon
     *
     *  @param[in]  bus     The bus to get the socket path for.
     *
     *  @return the path to the socket, or an empty string if no socket is
     *  available.
     */
    virtual std::string ipcDbusSocketPath(const BusType &bus) const = 0;
};

#endif // !defined(IDOBBYIPCUTILS_H)
