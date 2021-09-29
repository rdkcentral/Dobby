/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
 * IpcService.h
 *
 *  Created on: 5 Jun 2015
 *      Author: riyadh
 */

#ifndef AI_IPC_IPCSERVICE_H
#define AI_IPC_IPCSERVICE_H

#include "IpcCommon.h"
#include "IIpcService.h"

#include "DbusConnection.h"
#include "IDbusUserIdSenderIdCache.h"

#include <ThreadedDispatcher.h>
#include <Common/Interface.h>

#include <dbus/dbus.h>

#include <future>
#include <string>
#include <thread>
#include <list>
#include <mutex>
#include <set>
#include <atomic>

class IpcService : public AI_IPC::IIpcService
                 , public std::enable_shared_from_this<IpcService>
{
public:
    enum BusType
    {
        SessionBus,
        SystemBus
    };

public:
    IpcService(BusType busType, const std::string& serviceName, int defaultTimeoutMs = -1);
    IpcService(const std::string& dbusAddress, const std::string& serviceName, int defaultTimeoutMs = -1);

    ~IpcService() override;

    virtual bool isValid() const override;

    virtual std::shared_ptr<AI_IPC::IAsyncReplyGetter> invokeMethod(const AI_IPC::Method& method, const  AI_IPC::VariantList& args, int timeoutMs = -1) override;

    virtual bool invokeMethod(const AI_IPC::Method& method, const  AI_IPC::VariantList& args,  AI_IPC::VariantList& replyArgs, int timeoutMs = -1) override;

    virtual bool emitSignal(const AI_IPC::Signal& signal, const  AI_IPC::VariantList& args) override;

    virtual std::string registerMethodHandler(const AI_IPC::Method& method, const AI_IPC::MethodHandler& handler) override;

    virtual std::string registerSignalHandler(const AI_IPC::Signal& signal, const AI_IPC::SignalHandler& handler) override;

    virtual bool unregisterHandler(const std::string& regId) override;

    virtual bool enableMonitor(const std::set<std::string>& matchRules, const AI_IPC::MonitorHandler& handler) override;

    virtual bool disableMonitor() override;

    virtual void flush() override;

    virtual bool start() override;

    virtual bool stop() override;

    bool isRegisteredObjectPath(const std::string& path);

    virtual bool isServiceAvailable(const std::string& serviceName) const override;

    std::string getBusAddress() const override;

private:

    bool invokeMethodAndGetReply(DBusMessage *dbusSendMsg,  AI_IPC::VariantList& replyArgs);

    DBusHandlerResult handleDbusMessageCb(DBusMessage *message);

    DBusHandlerResult handleDbusMessage(DBusMessage *message);

    DBusHandlerResult handleDbusSignal(const AI_IPC::Signal& signal, const  AI_IPC::VariantList& argList);

    DBusHandlerResult handleDbusMethodCall(const AI_IPC::Method& method, const  AI_IPC::VariantList& argList, DBusMessage *message);

    void unregisterHandlers();

    void registerObjectPath(const std::string& path);

    void unregisterObjectPath(const std::string& path);

    bool isDbusMessageAllowed(const std::string& sender, const std::string& interface);

    std::string mServiceName;

    std::string mBusAddress;

    std::shared_ptr<AI_IPC::DbusConnection> mDbusConnection;

    std::map<std::string, int> mObjectPaths;

    std::map<std::string, std::pair<AI_IPC::Method, AI_IPC::MethodHandler>> mMethodHandlers;

    std::map<std::string, std::pair<AI_IPC::Signal, AI_IPC::SignalHandler>> mSignalHandlers;

    AICommon::ThreadedDispatcher mHandlerDispatcher;

    std::mutex mMutex;

    std::atomic<bool> mRunning;

    int mNextSignalHandlerRegId;

    const int mDefaultTimeoutMs;

    bool mValid;

#if (AI_BUILD_TYPE == AI_DEBUG)

    DBusHandlerResult handleDbusMonitorEvent(DBusMessage *dbusMsg);

    std::atomic<bool> mInMonitorMode;

    AI_IPC::MonitorHandler mMonitorCb;

    std::set<std::string> mMonitorMatchRules;

#endif // if (AI_BUILD_TYPE == AI_DEBUG)
};

#endif /* AI_IPC_IPCSERVICE_H */
