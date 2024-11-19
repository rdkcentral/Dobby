/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
 * File:   Dobby.h
 *
 */
#ifndef DOBBY_H
#define DOBBY_H

#include "ContainerId.h"

#include <IIpcService.h>

#include <signal.h>

#include <list>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <condition_variable>

class DobbyEnv;
class DobbyUtils;
class DobbyIPCUtils;
class DobbyManager;
class DobbyWorkQueue;
class DobbyLogger;
class IDobbySettings;

// -----------------------------------------------------------------------------
/**
 *  @class Dobby
 *  @brief The root Dobby object, runs the dbus loop.
 *
 *
 *
 */
class Dobby
{
public:
    Dobby(const std::string& dbusAddress,
          const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
          const std::shared_ptr<const IDobbySettings>& settings);
    ~Dobby();

public:
    void run() const;

public:
    static void configSignals();

public:
    enum LogTarget : unsigned { Console = 0x1, SysLog = 0x2, EthanLog = 0x4, Journald = 0x8 };
    static void setupLogging(unsigned targets = LogTarget::Console);

public:
    void setDefaultAIDbusAddresses(const std::string& aiPrivateBusAddress,
                                   const std::string& aiPublicBusAddress);

private:
    #define DOBBY_DBUS_METHOD(x) \
        void x(std::shared_ptr<AI_IPC::IAsyncReplySender> reply)

    DOBBY_DBUS_METHOD(ping);
    DOBBY_DBUS_METHOD(shutdown);
    DOBBY_DBUS_METHOD(setLogMethod);
    DOBBY_DBUS_METHOD(setLogLevel);
    DOBBY_DBUS_METHOD(setAIDbusAddress);

    DOBBY_DBUS_METHOD(startFromSpec);
    DOBBY_DBUS_METHOD(startFromBundle);
    DOBBY_DBUS_METHOD(stop);
    DOBBY_DBUS_METHOD(pause);
    DOBBY_DBUS_METHOD(resume);
    DOBBY_DBUS_METHOD(hibernate);
    DOBBY_DBUS_METHOD(wakeup);
    DOBBY_DBUS_METHOD(addMount);
    DOBBY_DBUS_METHOD(removeMount);
    DOBBY_DBUS_METHOD(exec);
    DOBBY_DBUS_METHOD(list);
    DOBBY_DBUS_METHOD(getState);
    DOBBY_DBUS_METHOD(getInfo);

#if defined(LEGACY_COMPONENTS) && (AI_BUILD_TYPE == AI_DEBUG)
    DOBBY_DBUS_METHOD(createBundle);
    DOBBY_DBUS_METHOD(getSpec);
#endif //defined(LEGACY_COMPONENTS) && (AI_BUILD_TYPE == AI_DEBUG)

#if (AI_BUILD_TYPE == AI_DEBUG)
    DOBBY_DBUS_METHOD(getOCIConfig);
#endif //(AI_BUILD_TYPE == AI_DEBUG)

#if defined(AI_ENABLE_TRACING)
    DOBBY_DBUS_METHOD(startInProcessTracing);
    DOBBY_DBUS_METHOD(stopInProcessTracing);
#endif

    DOBBY_DBUS_METHOD(addAnnotation);

    #undef DOBBY_DBUS_METHOD

private:
    void initIpcMethods();

#if defined(RDK)
    void initWatchdog();
    bool onWatchdogTimer();
#endif


private:
    void onContainerStarted(int32_t cd, const ContainerId& id);
    void onContainerStopped(int32_t cd, const ContainerId& id, int status);
    void onContainerHibernated(int32_t cd, const ContainerId& id);
    void onContainerAwoken(int32_t cd, const ContainerId& id);

private:
    void runWorkQueue() const;

private:
    std::shared_ptr<DobbyEnv> mEnvironment;
    std::shared_ptr<DobbyUtils> mUtilities;
    std::shared_ptr<DobbyIPCUtils> mIPCUtilities;
    std::shared_ptr<DobbyManager> mManager;
    std::unique_ptr<DobbyWorkQueue> mWorkQueue;

private:
    const std::shared_ptr<AI_IPC::IIpcService> mIpcService;
    const std::string mService;
    const std::string mObjectPath;
    std::list<std::string> mHandlers;

private:
    std::atomic<bool> mShutdown;

    int mWatchdogTimerId;

private:
    static void nullSigChildHandler(int sigNum, siginfo_t *info, void *context);

    static volatile sig_atomic_t mSigTerm;
    static void sigTermHandler(int sigNum);

private:
    static void logPrinter(int level, const char *file, const char *func,
                           int line, const char *message);
    static void logConsolePrinter(int level, const char *file, const char *func,
                                  int line, const char *message);
#if defined(RDK)
    static void logJournaldPrinter(int level, const char *file, const char *func,
                                   int line, const char *message);
#endif

    static std::atomic<unsigned> mLogTargets;
    static int mEthanLogPipeFd;
};


#endif // !defined(DOBBY_H)
