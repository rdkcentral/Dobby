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

#include "IIpcServiceMock.h"

void AI_IPC::IIpcService::setImpl(IIpcServiceImpl* newImpl)
{
    impl = newImpl;
}

bool AI_IPC::IIpcService::isValid()
{
   EXPECT_NE(impl, nullptr);

   return impl->isValid();
}

std::shared_ptr<AI_IPC::IAsyncReplyGetter> AI_IPC::IIpcService::invokeMethod(const Method& method, const VariantList& args, int timeoutMs)
{
   EXPECT_NE(impl, nullptr);

    return impl->invokeMethod(method, args, timeoutMs);
}

bool AI_IPC::IIpcService::invokeMethod(const Method& method, const VariantList& args, VariantList& replyArgs, int timeoutMs)
{
   EXPECT_NE(impl, nullptr);

    return impl->invokeMethod(method, args, replyArgs, timeoutMs);
}

bool AI_IPC::IIpcService::emitSignal(const Signal& signal, const VariantList& args)
{
   EXPECT_NE(impl, nullptr);

    return impl->emitSignal(signal, args);
}

std::string AI_IPC::IIpcService::registerMethodHandler(const Method& method, const MethodHandler& handler)
{
   EXPECT_NE(impl, nullptr);

    return impl->registerMethodHandler(method, handler);
}

std::string AI_IPC::IIpcService::registerSignalHandler(const Signal& signal, const SignalHandler& handler)
{
   EXPECT_NE(impl, nullptr);

    return impl->registerSignalHandler(signal, handler);
}

bool AI_IPC::IIpcService::unregisterHandler(const std::string& regId)
{
   EXPECT_NE(impl, nullptr);

    return impl->unregisterHandler(regId);
}

bool AI_IPC::IIpcService::enableMonitor(const std::set<std::string>& matchRules, const MonitorHandler& handler)
{
   EXPECT_NE(impl, nullptr);

    return impl->enableMonitor(matchRules, handler);
}

void AI_IPC::IIpcService::flush()
{
   EXPECT_NE(impl, nullptr);

    impl->flush();
}

