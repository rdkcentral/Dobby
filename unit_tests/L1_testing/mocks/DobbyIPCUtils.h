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

#ifndef DOBBYIPCUTILS_H
#define DOBBYIPCUTILS_H

#include <IIpcService.h>
#include "IDobbyIPCUtils.h"
#include <map>
#include <mutex>
#include <functional>


class DobbyIPCUtilsImpl
{
    public:
    virtual ~DobbyIPCUtilsImpl() = default;
    virtual bool setAIDbusAddress(bool privateBus, const std::string &address) = 0;
    virtual std::shared_ptr<AI_IPC::IAsyncReplyGetter> ipcInvokeMethod(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,int timeoutMs) const = 0;
    virtual bool ipcInvokeMethod(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,AI_IPC::VariantList &replyArgs) const = 0;
    virtual bool ipcEmitSignal(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::VariantList &args) const = 0;
    virtual bool ipcServiceAvailable(const IDobbyIPCUtils::BusType &bus,const std::string &serviceName) const = 0;
    virtual int ipcRegisterServiceHandler(const IDobbyIPCUtils::BusType &bus,const std::string &serviceName,const std::function<void(bool)> &handlerFunc) = 0;
    virtual int ipcRegisterSignalHandler(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::SignalHandler &handlerFunc) = 0;
    virtual void ipcUnregisterHandler(const IDobbyIPCUtils::BusType &bus, int handlerId) = 0;
    virtual std::string ipcDbusAddress(const IDobbyIPCUtils::BusType &bus) const = 0;
    virtual std::string ipcDbusSocketPath(const IDobbyIPCUtils::BusType &bus) const = 0;

};

class DobbyIPCUtils : public IDobbyIPCUtils
{

protected:
    static DobbyIPCUtilsImpl* impl;

public:
    DobbyIPCUtils(const std::string &systemDbusAddress,
               const std::shared_ptr<AI_IPC::IIpcService> &systemIpcService);
    DobbyIPCUtils();
    ~DobbyIPCUtils();

    static void setImpl(DobbyIPCUtilsImpl* newImpl);
    bool setAIDbusAddress(bool privateBus, const std::string &address);
    std::shared_ptr<AI_IPC::IAsyncReplyGetter> ipcInvokeMethod(const BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,int timeoutMs) const override;
    bool ipcInvokeMethod(const BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,AI_IPC::VariantList &replyArgs) const override;
    bool ipcEmitSignal(const BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::VariantList &args) const override;
    bool ipcServiceAvailable(const BusType &bus,const std::string &serviceName) const override;
    int ipcRegisterServiceHandler(const BusType &bus,const std::string &serviceName,const std::function<void(bool)> &handlerFunc) override;
    int ipcRegisterSignalHandler(const BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::SignalHandler &handlerFunc) override;
    void ipcUnregisterHandler(const BusType &bus, int handlerId) override;
    std::string ipcDbusAddress(const BusType &bus) const override;
    std::string ipcDbusSocketPath(const BusType &bus) const override;
};

#endif
