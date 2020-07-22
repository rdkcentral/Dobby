/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
//
//  SDBusIpcService.h
//  IpcService
//
//

#ifndef SDBUSIPCSERVICE_H
#define SDBUSIPCSERVICE_H

#include "IpcCommon.h"
#include "IIpcService.h"

#include <set>
#include <atomic>
#include <string>
#include <thread>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <queue>


typedef struct sd_bus sd_bus;
typedef struct sd_bus_slot sd_bus_slot;
typedef struct sd_bus_message sd_bus_message;
typedef struct sd_event_source sd_event_source;


class SDBusAsyncReplyGetter;


class SDBusIpcService : public AI_IPC::IIpcService
                      , public std::enable_shared_from_this<SDBusIpcService>
{
public:
    enum BusType
    {
        SessionBus,
        SystemBus
    };

public:
    SDBusIpcService(BusType busType, const std::string& serviceName, int defaultTimeoutMs = -1);
    SDBusIpcService(const std::string &busAddress, const std::string& serviceName, int defaultTimeoutMs = -1);
    ~SDBusIpcService() final;

    std::shared_ptr<AI_IPC::IAsyncReplyGetter> invokeMethod(const AI_IPC::Method &method, const AI_IPC::VariantList &args, int timeoutMs) override;
    bool invokeMethod(const AI_IPC::Method &method, const AI_IPC::VariantList &args, AI_IPC::VariantList &replyArgs, int timeoutMs) override;

    bool emitSignal(const AI_IPC::Signal &signal, const AI_IPC::VariantList &args) override;

    std::string registerMethodHandler(const AI_IPC::Method &method, const AI_IPC::MethodHandler &handler) override;
    std::string registerSignalHandler(const AI_IPC::Signal &signal, const AI_IPC::SignalHandler &handler) override;
    bool unregisterHandler(const std::string& regId) override;

    bool enableMonitor(const std::set<std::string> &matchRules, const AI_IPC::MonitorHandler& handler) override;
    bool disableMonitor() override;

    void flush() override;

    bool start() override;
    bool stop() override;

    bool isServiceAvailable(const std::string& serviceName) const override;

    std::string getBusAddress() const override;

private:
    friend class SDBusAsyncReplySender;
    void freeMethodReply(uint32_t replyId);
    bool sendMethodReply(uint32_t replyId,
                         const AI_IPC::VariantList& replyArgs);

    uid_t getSenderUid(const std::string &senderName);

private:
    bool init(const std::string &serviceName, int defaultTimeoutMs);

    void eventLoopThread();

    bool runOnEventLoopThread(std::function<void()> &&fn) const;

    static int onExecCall(sd_event_source *s, int fd, uint32_t revents, void *userdata);

    static int onRuleMatch(sd_bus_message *m, void *userData, void *retError);
    static int onMethodCall(sd_bus_message *reply, void *userData, void *retError);
    static int onMethodReply(sd_bus_message *reply, void *userData, void *retError);

private:
    uint64_t mDefaultTimeoutUsecs;

    std::thread mThread;
    sd_bus *mSDBus;

    std::atomic<bool> mStarted;

    struct RegisteredMethod
    {
        sd_bus_slot *objectSlot;
        std::string path;
        std::string interface;
        std::string name;
        AI_IPC::MethodHandler callback;

        RegisteredMethod(sd_bus_slot *slot,
                         const AI_IPC::Method &method,
                         const AI_IPC::MethodHandler &handler)
            : objectSlot(slot)
            , path(method.object), interface(method.interface), name(method.name)
            , callback(handler)
        { }
    };

    struct RegisteredSignal
    {
        sd_bus_slot *matchSlot;
        AI_IPC::SignalHandler callback;

        RegisteredSignal(sd_bus_slot *slot,
                         const AI_IPC::SignalHandler &handler)
            : matchSlot(slot)
            , callback(handler)
        { }
    };

    uint64_t mHandlerTag;
    std::map<std::string, RegisteredMethod> mMethodHandlers;
    std::map<std::string, RegisteredSignal> mSignalHandlers;

    struct Executor
    {
        uint64_t tag;
        std::function<void()> func;

        Executor(uint64_t t, std::function<void()> &&f)
            : tag(t), func(std::move(f))
        { }
    };

    mutable uint64_t mExecCounter;
    uint64_t mLastExecTag;
    int mExecEventFd;
    mutable std::mutex mExecLock;
    mutable std::condition_variable mExecCond;
    mutable std::deque< Executor > mExecQueue;

    std::map< uint64_t, std::shared_ptr<SDBusAsyncReplyGetter> > mCalls;
    std::map< uint32_t, sd_bus_message* > mCallReplies;
    std::queue<uint32_t> mReplyIdentifiers;

};

#endif // SDBUSIPCSERVICE_H
