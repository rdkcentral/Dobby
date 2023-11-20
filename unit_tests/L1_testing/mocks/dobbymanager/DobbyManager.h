/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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

#ifndef DOBBYMANAGER_H
#define DOBBYMANAGER_H

#include "IDobbyUtils.h"
#include "IDobbyIPCUtils.h"
#include "DobbyConfig.h"
#include "IDobbyRdkPlugin.h"
#include "IDobbyRdkLoggingPlugin.h"
#include "ContainerId.h"
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
class DobbyStartState;
class DobbyLegacyPluginManager;
class DobbyConfig;

class DobbyContainer;

class DobbyManagerImpl {
public:
    virtual ~DobbyManagerImpl() = default;

#if defined(LEGACY_COMPONENTS)

    virtual int32_t startContainerFromSpec(const ContainerId& id,
                                          const std::string& jsonSpec,
                                          const std::list<int>& files,
                                          const std::string& command,
                                          const std::string& displaySocket,
                                          const std::vector<std::string>& envVars) = 0;

    virtual std::string specOfContainer(int32_t cd) const = 0;

    virtual bool createBundle(const ContainerId& id, const std::string& jsonSpec) = 0;

#endif //defined(LEGACY_COMPONENTS)


    virtual int32_t startContainerFromBundle(const ContainerId& id,
                                            const std::string& bundlePath,
                                            const std::list<int>& files,
                                            const std::string& command,
                                            const std::string& displaySocket,
                                            const std::vector<std::string>& envVars) = 0;

    virtual bool stopContainer(int32_t cd, bool withPrejudice) = 0;

    virtual bool pauseContainer(int32_t cd) = 0;

    virtual bool resumeContainer(int32_t cd) = 0;

    virtual bool execInContainer(int32_t cd,
                             const std::string& options,
                             const std::string& command) = 0;

    virtual std::list<std::pair<int32_t, ContainerId>> listContainers() const = 0;

    virtual int32_t stateOfContainer(int32_t cd) const = 0;

    virtual std::string statsOfContainer(int32_t cd) const = 0;

    virtual std::string ociConfigOfContainer(int32_t cd) const = 0;

};

class DobbyManager {

protected:

    static DobbyManagerImpl* impl;


public:
    typedef std::function<void(int32_t cd, const ContainerId& id)> ContainerStartedFunc;
    typedef std::function<void(int32_t cd, const ContainerId& id, int32_t status)> ContainerStoppedFunc;
    DobbyManager();
    DobbyManager(std::shared_ptr<DobbyEnv>&, std::shared_ptr<DobbyUtils>&, std::shared_ptr<DobbyIPCUtils>&, const std::shared_ptr<const IDobbySettings>&, std::function<void(int, const ContainerId&)>&, std::function<void(int, const ContainerId&, int)>&);
    ~DobbyManager();

    static void setImpl(DobbyManagerImpl* newImpl);
    static DobbyManager* getInstance();

#if defined(LEGACY_COMPONENTS)

    static int32_t startContainerFromSpec(const ContainerId& id,
                                          const std::string& jsonSpec,
                                          const std::list<int>& files,
                                          const std::string& command,
                                          const std::string& displaySocket,
                                          const std::vector<std::string>& envVars);
    static std::string specOfContainer(int32_t cd);
    static bool createBundle(const ContainerId& id, const std::string& jsonSpec);
#endif //defined(LEGACY_COMPONENTS)

    static int32_t startContainerFromBundle(const ContainerId& id,
                                            const std::string& bundlePath,
                                            const std::list<int>& files,
                                            const std::string& command,
                                            const std::string& displaySocket,
                                            const std::vector<std::string>& envVars);
    static bool stopContainer(int32_t cd, bool withPrejudice);
    static bool pauseContainer(int32_t cd);
    static bool resumeContainer(int32_t cd);
    static bool execInContainer(int32_t cd,
                                const std::string& options,
                                const std::string& command);
    static std::list<std::pair<int32_t, ContainerId>> listContainers();
    static int32_t stateOfContainer(int32_t cd);
    static std::string statsOfContainer(int32_t cd);
    static std::string ociConfigOfContainer(int32_t cd);

};

#endif // !defined(DOBBYMANAGER_H)

