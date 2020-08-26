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
 * File:   DobbyManager.h
 *
 */
#ifndef DOBBYMANAGER_H
#define DOBBYMANAGER_H

#include "IDobbyUtils.h"
#include "IDobbyIPCUtils.h"
#include "DobbyConfig.h"
#include "IDobbyRdkPlugin.h"
#include "IDobbyRdkLoggingPlugin.h"
#include "ContainerId.h"
#include "DobbyLogger.h"
#include <IIpcService.h>

#include <pthread.h>

#include <map>
#include <list>
#include <cstdint>
#include <mutex>
#include <thread>
#include <string>
#include <memory>
#include <future>
#include <functional>
#include <netinet/in.h>

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

class IDobbyEnv;
class IDobbySettings;
class DobbyRunC;
class DobbyStartState;
class IDobbySysHook;
class DobbyPluginManager;
class DobbyState;
class DobbyConfig;

class DobbyContainer;


// -----------------------------------------------------------------------------
/**
 *  @class DobbyManager
 *  @brief The main object which starts / stops / manages the containers
 *
 *  This is where most of the work is done for creating and monitoring
 *  containers.
 *
 *
 *  https://groups.google.com/a/opencontainers.org/forum/m/#!topic/dev/Y7p6YW8zr4s
 */
class DobbyManager
{
public:
    typedef std::function<void(int32_t cd, const ContainerId& id)> ContainerStartedFunc;
    typedef std::function<void(int32_t cd, const ContainerId& id, int32_t status)> ContainerStoppedFunc;

public:
    DobbyManager(const std::shared_ptr<IDobbyEnv>& env,
                 const std::shared_ptr<IDobbyUtils>& utils,
                 const std::shared_ptr<IDobbyIPCUtils>& ipcUtils,
                 const std::shared_ptr<DobbyLogger> logger,
                 const std::shared_ptr<const IDobbySettings>& settings,
                 const ContainerStartedFunc& containerStartedCb,
                 const ContainerStoppedFunc& containerStoppedCb);
    ~DobbyManager();

private:
    void setupSystem();
    void setupWorkspace(const std::shared_ptr<IDobbyEnv>& env);

#if defined(LEGACY_COMPONENTS)
    void setupSystemHooks();
#endif // defined(LEGACY_COMPONENTS)

    void cleanupContainers();

public:
#if defined(LEGACY_COMPONENTS)
    int32_t startContainerFromSpec(const ContainerId& id,
                                   const std::string& jsonSpec,
                                   const std::list<int>& files,
                                   const std::string& command,
                                   const std::string& displaySocket);
#endif // defined(LEGACY_COMPONENTS)

    int32_t startContainerFromBundle(const ContainerId& id,
                                     const std::string& bundlePath,
                                     const std::list<int>& files,
                                     const std::string& command,
                                     const std::string& displaySocket);

    bool stopContainer(int32_t cd, bool withPrejudice);

    bool pauseContainer(int32_t cd);
    bool resumeContainer(int32_t cd);

    bool execInContainer(int32_t cd,
                         const std::string& options,
                         const std::string& command);

    std::list<std::pair<int32_t, ContainerId>> listContainers() const;

    int32_t stateOfContainer(int32_t cd) const;

    std::string statsOfContainer(int32_t cd) const;

    uint32_t getBridgeConnections();
    uint32_t getIpAddress(const std::string &vethName);
    bool freeIpAddress(in_addr_t address);
    std::vector<std::string> getExtIfaces();

public:
    std::string jsonConfigOfContainer(int32_t cd) const;

#if defined(LEGACY_COMPONENTS)
    std::string specOfContainer(int32_t cd) const;
    bool createBundle(const ContainerId& id, const std::string& jsonSpec);
#endif // defined(LEGACY_COMPONENTS)

private:
    void onChildExit();

private:
    std::shared_ptr<IDobbyRdkLoggingPlugin>
            GetContainerLogger(const std::unique_ptr<DobbyContainer> &container);

    bool createAndStart(const ContainerId &id,
                        const std::unique_ptr<DobbyContainer> &container,
                        const std::list<int> &files);

    std::string createCustomConfig(const std::unique_ptr<DobbyContainer> &container,
                                   const std::string &command,
                                   const std::string &displaySocket);

    bool createAndStartContainer(const ContainerId& id,
                                 const std::unique_ptr<DobbyContainer>& container,
                                 const std::list<int>& files,
                                 const std::string& command = "",
                                 const std::string& displaySocket = "");

    bool restartContainer(const ContainerId& id,
                          const std::unique_ptr<DobbyContainer>& container);

private:
    ContainerStartedFunc mContainerStartedCb;
    ContainerStoppedFunc mContainerStoppedCb;

private:
    mutable std::mutex mLock;
    std::map<ContainerId, std::unique_ptr<DobbyContainer>> mContainers;
    std::multimap<ContainerId, pid_t> mContainerExecPids;

private:
    bool onPostInstallationHook(const std::unique_ptr<DobbyContainer> &container);
    bool onPreCreationHook(const std::unique_ptr<DobbyContainer> &container);
    bool onPostHaltHook(const std::unique_ptr<DobbyContainer> &container);

#if defined(LEGACY_COMPONENTS)
    bool onPostConstructionHook(const ContainerId& id,
                                const std::shared_ptr<DobbyStartState>& startState,
                                const std::unique_ptr<DobbyContainer>& container);
    bool onPreStartHook(const ContainerId& id,
                        const std::unique_ptr<DobbyContainer>& container);
    bool onPostStartHook(const ContainerId& id,
                         const std::unique_ptr<DobbyContainer>& container);
    bool onPostStopHook(const ContainerId& id,
                        const std::unique_ptr<DobbyContainer>& container);
    bool onPreDestructionHook(const ContainerId& id,
                              const std::unique_ptr<DobbyContainer>& container);

private:
    enum class HookType {
        PostConstruction, PreStart, PostStart, PostStop, PreDestruction };

private:
    typedef std::function<bool (IDobbySysHook*)> SysHookFn;
    bool executeSysHooks(const std::unique_ptr<DobbyContainer>& container,
                         const HookType& hookType,
                         const SysHookFn& sysHookFn);
#endif // defined(LEGACY_COMPONENTS)

private:
    void startRuncMonitorThread();
    void stopRuncMonitorThread();
    void runcMonitorThread();

private:
    const std::shared_ptr<IDobbyEnv> mEnvironment;
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::shared_ptr<IDobbyIPCUtils> mIPCUtilities;
    const std::shared_ptr<const IDobbySettings> mSettings;
    const std::shared_ptr<DobbyLogger> mLogger;

private:
    std::unique_ptr<DobbyRunC> mRunc;
    std::shared_ptr<DobbyState> mState;
#if defined(LEGACY_COMPONENTS)
    std::unique_ptr<DobbyPluginManager> mPlugins;
#endif // defined(LEGACY_COMPONENTS)

private:
    std::list<std::shared_ptr<IDobbySysHook>> mSysHooks;

private:
    std::thread mRuncMonitorThread;
    std::atomic<bool> mRuncMonitorTerminate;

};


#endif // !defined(DOBBYMANAGER_H)
