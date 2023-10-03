#pragma once

#include "gmock/gmock.h"
#include "IIpcService.h"
#include "IpcCommon.h"

namespace AI_IPC
{
class IpcServiceMock : public AI_IPC::IIpcService {
public:
    MOCK_METHOD(bool, isValid, (), (const, override));
    MOCK_METHOD(std::shared_ptr<IAsyncReplyGetter>, invokeMethod, (const Method& method, const VariantList& args, int timeoutMs), (override));
    MOCK_METHOD(bool, invokeMethod, (const Method& method, const VariantList& args, VariantList& replyArgs, int timeoutMs), (override));
    MOCK_METHOD(std::string, registerMethodHandler, (const Method& method, const MethodHandler& handler), (override));
    MOCK_METHOD(bool, emitSignal, (const Signal& signal, const VariantList& args), (override));
    MOCK_METHOD(std::string, registerSignalHandler, (const Signal& signal, const SignalHandler& handler), (override));
    MOCK_METHOD(bool, unregisterHandler, (const std::string& regId), (override));
    MOCK_METHOD(bool, enableMonitor, (const std::set<std::string>& matchRules, const MonitorHandler& handler), (override));
    MOCK_METHOD(bool, disableMonitor, (), (override));
    MOCK_METHOD(bool, isServiceAvailable, (const std::string& serviceName), (const, override));
    MOCK_METHOD(void, flush, (), (override));
    MOCK_METHOD(bool, start, (), (override));
    MOCK_METHOD(bool, stop, (), (override));
    MOCK_METHOD(std::string, getBusAddress, (), (const, override));

     };
}
