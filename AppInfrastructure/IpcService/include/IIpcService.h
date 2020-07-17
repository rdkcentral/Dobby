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
 * IIpcService.h
 *
 *  Created on: 3 Jun 2015
 *      Author: riyadh
 */

#ifndef AI_IPC_IIPCSERVICE_H
#define AI_IPC_IIPCSERVICE_H

#include "IpcCommon.h"

#include <memory>
#include <string>
#include <set>

namespace AI_IPC
{

/**
 * @brief IPC service that enables us to invoke remote method and emit signals as well as to handle incoming method calls and received signals.
 */
class IIpcService
{
public:
    virtual ~IIpcService() = default;

    /**
     * @brief Invoke a method and get reply asynchronously
     *
     * @parameter[in]   method         Method definition
     * @parameter[in]   args           Method arguments
     * @parameter[in]   timeoutMs      Timeout in milliseconds, -1 for default (30 seconds)
     *
     * @returns On success: Shared pointer pointing to a reply getter to receive reply asynchronously.
     * @returns On failure: Empty shared pointer.
     */
    virtual std::shared_ptr<IAsyncReplyGetter> invokeMethod(const Method& method, const VariantList& args, int timeoutMs = -1) = 0;

    /**
     * @brief Invoke a method and get reply synchronously
     *
     * @parameter[in]   method         Method definition
     * @parameter[in]   args           Method arguments
     * @parameter[in]   replyArgs      Reply return by the method call
     * @parameter[in]   timeoutMs      Timeout in milliseconds, -1 for default (30 seconds)
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool invokeMethod(const Method& method, const VariantList& args, VariantList& replyArgs, int timeoutMs = -1) = 0;

    /**
     * @brief Emit a signal
     *
     * @parameter[in]   signal         Signal definition
     * @parameter[in]   args           Signal arguments/data
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool emitSignal(const Signal& signal, const VariantList& args) = 0;

    /**
     * @brief Register a method handler
     *
     * @parameter[in]   method         Method definition
     * @parameter[in]   handler        Method handler
     *
     * @returns On success: Registration ID.
     * @returns On failure: Empty string.
     */
    virtual std::string registerMethodHandler(const Method& method, const MethodHandler& handler) = 0;

    /**
     * @brief Register a signal handler
     *
     * @parameter[in]   method         Signal definition
     * @parameter[in]   handler        Signal handler
     *
     * @returns On success: Registration ID.
     * @returns On failure: Empty string.
     */
    virtual std::string registerSignalHandler(const Signal& signal, const SignalHandler& handler) = 0;

    /**
     * @brief Unregister a method or signal handler
     *
     * @parameter[in]   regId         Registration Id
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool unregisterHandler(const std::string& regId) = 0;

    /**
     * @brief Enables monitor mode for the service
     *
     * @parameter[in]   matchRules    Optional set of match rules for monitor mode, can be empty
     * @parameter[in]   handler       Handler callback for all events received in monitor mode
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool enableMonitor(const std::set<std::string>& matchRules, const MonitorHandler& handler) = 0;

    /**
     * @brief Disables monitor mode for the service
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool disableMonitor() = 0;

    /**
     * @brief Checks if the given service name is currently registered on the bus.
     *
     * @parameter[in]   serviceName     The name of the service to look for
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool isServiceAvailable(const std::string& serviceName) const = 0;

    /**
     * @brief Flushes all messages out
     *
     * This method ensures that any message or signal handlers queued before this function was called are
     * processed before the function returns.
     *
     * For obvious reasons do not hold any lock that a handler might need while calling this function.
     */
    virtual void flush() = 0;

    /**
     * @brief Start IPC service
     *
     * It needs to be invoked to start the event dispatcher, which is required to handle method and signals,
     * as well as to get method call reply.
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool start() = 0;

    /**
     * @brief Stop IPC service
     *
     * The event dispatcher thread will be terminated.
     *
     * @returns On success: True.
     * @returns On failure: False.
     */
    virtual bool stop() = 0;

    /**
     * @brief Returns the dbus address the service is using.
     *
     * Note the address is formatted like a dbus address and is NOT just the path to the unix socket.
     *
     * @returns The dbus address.
     */
    virtual std::string getBusAddress() const = 0;
};

}

#endif /* AI_IPC_IIPCSERVICE_H */

