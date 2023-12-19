/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "DobbyIPCUtilsMock.h"

DobbyIPCUtils::DobbyIPCUtils(const std::string &systemDbusAddress,
           const std::shared_ptr<AI_IPC::IIpcService> &systemIpcService)
{
}

DobbyIPCUtils::DobbyIPCUtils()
{
}

DobbyIPCUtils::~DobbyIPCUtils()
{
}

void DobbyIPCUtils::setImpl(DobbyIPCUtilsImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

bool DobbyIPCUtils::setAIDbusAddress(bool privateBus, const std::string &address)
{
   EXPECT_NE(impl, nullptr);

    return impl->setAIDbusAddress(privateBus,address);
}

std::shared_ptr<AI_IPC::IAsyncReplyGetter> DobbyIPCUtils::ipcInvokeMethod(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,int timeoutMs) const
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcInvokeMethod(bus,method,args,timeoutMs);
}

bool DobbyIPCUtils::ipcInvokeMethod(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,AI_IPC::VariantList &replyArgs) const
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcInvokeMethod(bus,method,args,replyArgs);
}

bool DobbyIPCUtils::ipcEmitSignal(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::VariantList &args) const
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcEmitSignal(bus,signal,args);
}

bool DobbyIPCUtils::ipcServiceAvailable(const IDobbyIPCUtils::BusType &bus,const std::string &serviceName) const
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcServiceAvailable(bus,serviceName);
}

int DobbyIPCUtils::ipcRegisterServiceHandler(const IDobbyIPCUtils::BusType &bus,const std::string &serviceName,const std::function<void(bool)> &handlerFunc)
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcRegisterServiceHandler(bus,serviceName,handlerFunc);
}

int DobbyIPCUtils::ipcRegisterSignalHandler(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::SignalHandler &handlerFunc)
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcRegisterSignalHandler(bus,signal,handlerFunc);
}

void DobbyIPCUtils::ipcUnregisterHandler(const IDobbyIPCUtils::BusType &bus, int handlerId)
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcUnregisterHandler(bus,handlerId);
}

std::string DobbyIPCUtils::ipcDbusAddress(const IDobbyIPCUtils::BusType &bus) const
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcDbusAddress(bus);
}

std::string DobbyIPCUtils::ipcDbusSocketPath(const IDobbyIPCUtils::BusType &bus) const
{
   EXPECT_NE(impl, nullptr);

    return impl->ipcDbusSocketPath(bus);
}

