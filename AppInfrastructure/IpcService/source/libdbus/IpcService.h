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
#include "IDbusPackageEntitlements.h"
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

namespace AI_DBUS
{

class IDbusServer;

}

namespace packagemanager
{
    class IPackageManager;
}

namespace AI_IPC
{

class IpcService : public IIpcService
{
public:
    enum class BusType
    {
        SessionBus,
        SystemBus
    };

public:
    IpcService(const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer, const std::string& serviceName, int defaultTimeoutMs = -1);

    IpcService( const std::shared_ptr<const AI_DBUS::IDbusServer>& dbusServer,
                const std::string& serviceName,
                const std::shared_ptr<packagemanager::IPackageManager> &packageManager,
                bool dbusEntitlementCheckNeeded = false,
                int defaultTimeoutMs = -1);

    IpcService(BusType busType, const std::string& serviceName, int defaultTimeoutMs = -1);

    IpcService(const std::string& dbusAddress, const std::string& serviceName, int defaultTimeoutMs = -1);

    ~IpcService() override;

    virtual std::shared_ptr<IAsyncReplyGetter> invokeMethod(const Method& method, const VariantList& args, int timeoutMs = -1) override;

    virtual bool invokeMethod(const Method& method, const VariantList& args, VariantList& replyArgs, int timeoutMs = -1) override;

    virtual bool emitSignal(const Signal& signal, const VariantList& args) override;

    virtual std::string registerMethodHandler(const Method& method, const MethodHandler& handler) override;

    virtual std::string registerSignalHandler(const Signal& signal, const SignalHandler& handler) override;

    virtual bool unregisterHandler(const std::string& regId) override;

    virtual bool enableMonitor(const std::set<std::string>& matchRules, const MonitorHandler& handler) override;

    virtual bool disableMonitor() override;

    virtual void flush() override;

    virtual bool start() override;

    virtual bool stop() override;

    bool isRegisteredObjectPath(const std::string& path);

    virtual bool isServiceAvailable(const std::string& serviceName) const override;

private:

    bool invokeMethodAndGetReply(DBusMessage *dbusSendMsg, VariantList& replyArgs);

    DBusHandlerResult handleDbusMessageCb(DBusMessage *message);

    DBusHandlerResult handleDbusMessage(DBusMessage *message);

    DBusHandlerResult handleDbusSignal(const Signal& signal, const VariantList& argList);

    DBusHandlerResult handleDbusMethodCall(const Method& method, const VariantList& argList, DBusMessage *message);

    void unregisterHandlers();

    void registerObjectPath(const std::string& path);

    void unregisterObjectPath(const std::string& path);

    bool isDbusMessageAllowed(const std::string& sender, const std::string& interface);

    const std::shared_ptr<const AI_DBUS::IDbusServer> mDbusServer;

    std::string mServiceName;

    std::shared_ptr<DbusConnection> mDbusConnection;

    std::map<std::string, int> mObjectPaths;

    std::map<std::string, std::pair<Method, MethodHandler>> mMethodHandlers;

    std::map<std::string, std::pair<Signal, SignalHandler>> mSignalHandlers;

    AICommon::ThreadedDispatcher mHandlerDispatcher;

    std::mutex mMutex;

    std::atomic<bool> mRunning;

    int mNextSignalHandlerRegId;

    const int mDefaultTimeoutMs;

#if (AI_BUILD_TYPE == AI_DEBUG)

    DBusHandlerResult handleDbusMonitorEvent(DBusMessage *dbusMsg);

    std::atomic<bool> mInMonitorMode;

    MonitorHandler mMonitorCb;

    std::set<std::string> mMonitorMatchRules;

#endif // if (AI_BUILD_TYPE == AI_DEBUG)

    std::shared_ptr<IDbusPackageEntitlements> mDbusPackageEntitlements;

    std::shared_ptr<IDbusUserIdSenderIdCache> mDbusUserIdSenderIdCache;

    bool mDbusEntitlementCheckNeeded;
};

}

#endif /* AI_IPC_IPCSERVICE_H */
