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

#include "gmock/gmock.h"
#include "IIpcService.h"

namespace AI_IPC
{

class IpcServiceMock : public IIpcServiceImpl {

public:
    IpcServiceMock(){}
    virtual ~IpcServiceMock() = default;
    MOCK_METHOD(bool, isValid, (), (const, override));
    MOCK_METHOD(std::shared_ptr<IAsyncReplyGetter>, invokeMethod, (const Method& method, const VariantList& args, int timeoutMs), (override));
    MOCK_METHOD(bool, invokeMethod, (const Method& method, const VariantList& args, VariantList& replyArgs, int timeoutMs), (override));
    MOCK_METHOD(std::string, registerMethodHandler, (const Method& method, const MethodHandler& handler), (override));
    MOCK_METHOD(bool, emitSignal, (const Signal& signal, const VariantList& args), (override));
    MOCK_METHOD(std::string, registerSignalHandler, (const Signal& signal, const SignalHandler& handler), (override));
    MOCK_METHOD(bool, unregisterHandler, (const std::string& regId), (override));
    MOCK_METHOD(bool, enableMonitor, (const std::set<std::string>& matchRules, const MonitorHandler& handler), (override));
    MOCK_METHOD(void, flush, (), (override));
};

}//namespace AI_PIC
