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

#include "DobbyRdkPluginProxy.h"

#include <DobbyProtocol.h>

#include <Logging.h>

#include <thread>

// -----------------------------------------------------------------------------
/**
 *  @brief Registers the signal handlers.
 *
 */
DobbyRdkPluginProxy::DobbyRdkPluginProxy(const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
                                         const std::string& serviceName,
                                         const std::string& objectName)
    : mIpcService(ipcService)
    , mServiceName(serviceName)
    , mObjectName(objectName)
{
}

DobbyRdkPluginProxy::~DobbyRdkPluginProxy()
{
}

// -----------------------------------------------------------------------------
/**
 *  @brief Invokes a dbus method on the daemon.
 *
 *  The method is invoked with the service name and object name that was set
 *  in the constructor.
 *
 *
 *  @param[in]  interface_      The dbus interface name of the method
 *  @param[in]  method_         The dbus method name to invoke
 *  @param[in]  params_         The list of args to apply
 *  @param[out] returns_        Reference variable that the results will be put
 *                              in on success.
 *
 *  @return true on success, false on failure.
 */
bool DobbyRdkPluginProxy::invokeMethod(const char *interface_,
                              const char *method_,
                              const AI_IPC::VariantList& params_,
                              AI_IPC::VariantList& returns_) const
{
    const AI_IPC::Method method(mServiceName, mObjectName, interface_, method_);
    if (!mIpcService->invokeMethod(method, params_, returns_, 5000))
    {
        AI_LOG_ERROR("failed to invoke '%s.%s'", method.interface.c_str(),
                     method.name.c_str());
        return false;
    }
    else
    {
        return true;
    }
}


// -----------------------------------------------------------------------------
/**
 *  @brief Gets the number of veth interfaces connected through bridge
 *
 *  @return number of interfaces connected
 */
int32_t DobbyRdkPluginProxy::getBridgeConnections() const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    AI_IPC::VariantList returns;

    int32_t result;

    if (invokeMethod(DOBBY_RDKPLUGIN_INTERFACE,
                     DOBBY_RDKPLUGIN_GET_BRIDGE_CONNECTIONS,
                     {}, returns))
    {
        AI_IPC::parseVariantList<int32_t>(returns, &result);
    }

    AI_LOG_FN_EXIT();
    return result;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Picks the next available ip address from the pool of addresses and
 *  registers it for the given veth.
 *
 *  @param[in]  vethName    name of the veth pair to register with the address
 *
 *  @return returns free ip address from the pool, 0 if none available
 */
uint32_t DobbyRdkPluginProxy::getIpAddress(const std::string &vethName) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { vethName };
    AI_IPC::VariantList returns;

    uint32_t result = 0;

    if (invokeMethod(DOBBY_RDKPLUGIN_INTERFACE,
                     DOBBY_RDKPLUGIN_GET_ADDRESS,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<uint32_t>(returns, &result))
        {
            result = 0;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Adds the address back to the pool of available addresses, freeing it
 *  for use by other containers.
 *
 *  @param[in]  address     address to return to the pool of available addresses
 *
 *  @return true on success, false on failure.
 */
bool DobbyRdkPluginProxy::freeIpAddress(uint32_t address) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { address };
    AI_IPC::VariantList returns;

    bool result;

    if (invokeMethod(DOBBY_RDKPLUGIN_INTERFACE,
                     DOBBY_RDKPLUGIN_FREE_ADDRESS,
                     params, returns))
    {
        AI_IPC::parseVariantList<bool>(returns, &result);
    }

    AI_LOG_FN_EXIT();
    return result;
}


// -----------------------------------------------------------------------------
/**
 *  @brief Gets the external interfaces
 *
 *  @return external interfaces defined in dobby settings
 */
std::vector<std::string> DobbyRdkPluginProxy::getExternalInterfaces() const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    AI_IPC::VariantList returns;

    std::vector<std::string> result;

    if (invokeMethod(DOBBY_RDKPLUGIN_INTERFACE,
                     DOBBY_RDKPLUGIN_GET_EXT_IFACES,
                     {}, returns))
    {
        AI_IPC::parseVariantList<std::vector<std::string>>(returns, &result);
    }

    AI_LOG_FN_EXIT();
    return result;
}
