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
#pragma once

#include "DobbyIPCUtils.h"
#include <gmock/gmock.h>

class DobbyIPCUtilsMock : public DobbyIPCUtilsImpl {
public:
    virtual ~DobbyIPCUtilsMock() = default;

    MOCK_METHOD(bool, setAIDbusAddress,(bool privateBus, const std::string& address), (override));
    MOCK_METHOD(std::shared_ptr<AI_IPC::IAsyncReplyGetter>, ipcInvokeMethod,(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,int timeoutMs), (const,override));
    MOCK_METHOD(bool, ipcInvokeMethod,(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Method &method,const AI_IPC::VariantList &args,AI_IPC::VariantList &replyArgs), (const,override));
    MOCK_METHOD(bool, ipcEmitSignal,(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::VariantList &args), (const,override));
    MOCK_METHOD(bool, ipcServiceAvailable,(const IDobbyIPCUtils::BusType &bus,const std::string &serviceName), (const,override));
    MOCK_METHOD(int, ipcRegisterServiceHandler,(const IDobbyIPCUtils::BusType &bus,const std::string &serviceName,const std::function<void(bool)> &handlerFunc), (override));
    MOCK_METHOD(int, ipcRegisterSignalHandler,(const IDobbyIPCUtils::BusType &bus,const AI_IPC::Signal &signal,const AI_IPC::SignalHandler &handlerFunc), (override));
    MOCK_METHOD(void, ipcUnregisterHandler,(const IDobbyIPCUtils::BusType &bus, int handlerId), (override));
    MOCK_METHOD(std::string, ipcDbusAddress,(const IDobbyIPCUtils::BusType &bus), (const,override));
    MOCK_METHOD(std::string, ipcDbusSocketPath,(const IDobbyIPCUtils::BusType &bus), (const,override));

};

