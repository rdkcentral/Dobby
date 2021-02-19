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
 * IDobbyProxy.h
 * Author:
 *
 */
#ifndef IDOBBYPROXY_H
#define IDOBBYPROXY_H

#if defined(DOBBY_BUILD)
#include <Common/Notifier.h>
#else
#include <Dobby/Public/Common/Notifier.h>
#endif

#include <cstdint>
#include <list>
#include <string>
#include <memory>
#include <chrono>

namespace AI_IPC
{
    class IIpcService;
}



// -----------------------------------------------------------------------------
/**
 *  @interface IDobbyProxyEvents
 *  @brief Interface for the AI notifier API.
 *
 *  Contains a single event notifier method that is called whenever either
 *  of the 'org.rdk.dobby.ctrl1.Started' or 'org.rdk.dobby.ctrl1.Stopped'
 *  signals are received.
 *
 */
class IDobbyProxyEvents
{
public:
    enum class ContainerState
    {
        Invalid = 0,
        Starting = 1,
        Running = 2,
        Stopping = 3,
        Paused = 4,
        Stopped = 5,
    };

public:
    virtual ~IDobbyProxyEvents() = default;

    virtual void containerStateChanged(int32_t descriptor, const std::string& id, ContainerState newState) = 0;
};



// -----------------------------------------------------------------------------
/**
 *  @interface IDobbyProxy
 *  @brief Wrapper around an IpcService object that provides simpler method
 *  calls to the Dobby 'hypervisor' daemon.
 *
 *  All the methods are constant because the class doesn't have any internal
 *  state.
 *
 *
 */
class IDobbyProxy : public AICommon::Notifier<IDobbyProxyEvents>
{
public:
    ~IDobbyProxy() override = default;

public:
    // Admin interface
    virtual bool shutdown() const = 0;

    virtual bool ping() const = 0;

    virtual bool isAlive(const std::chrono::milliseconds& timeout) const = 0;

    virtual bool setLogMethod(uint32_t method, int pipeFd) const = 0;

    virtual bool setLogLevel(int level) const = 0;

    virtual bool setAIDbusAddress(bool privateBus,
                                  const std::string& address) const = 0;

    inline bool isAlive() const
    {
        return isAlive(std::chrono::milliseconds::min());
    }

    inline int32_t setLogMethod(uint32_t method) const
    {
        return setLogMethod(method, -1);
    }


public:
    // Control interface

    virtual int32_t startContainerFromSpec(const std::string& id,
                                           const std::string& jsonSpec,
                                           const std::list<int>& files,
                                           const std::string& command = "",
                                           const std::string& displaySocket = "",
                                           const std::vector<std::string>& envVars = std::vector<std::string>()) const = 0;


    virtual int32_t startContainerFromBundle(const std::string& id,
                                             const std::string& bundlePath,
                                             const std::list<int>& files,
                                             const std::string& command = "",
                                             const std::string& displaySocket = "",
                                             const std::vector<std::string>& envVars = std::vector<std::string>()) const = 0;

    virtual bool stopContainer(int32_t descriptor,
                               bool withPrejudice) const = 0;

    virtual bool pauseContainer(int32_t descriptor) const = 0;

    virtual bool resumeContainer(int32_t descriptor) const = 0;

    virtual bool execInContainer(int32_t cd,
                                 const std::string& options,
                                 const std::string& command) const = 0;

    virtual int getContainerState(int32_t descriptor) const = 0;

    virtual std::string getContainerInfo(int32_t descriptor) const = 0;

    virtual std::list<std::pair<int32_t, std::string>> listContainers() const = 0;


    inline int32_t startContainerFromSpec(const std::string& id,
                                          const std::string& jsonSpec) const
    {
        return startContainerFromSpec(id, jsonSpec, { });
    }

    inline int32_t startContainerFromBundle(const std::string& id,
                                            const std::string& bundlePath) const
    {
        return startContainerFromBundle(id, bundlePath, { });
    }

    inline bool stopContainer(int32_t descriptor) const
    {
        return stopContainer(descriptor, false);
    }

public:
    typedef std::function<void(int32_t, const std::string&, IDobbyProxyEvents::ContainerState, const void*)> StateChangeListener;

    virtual int registerListener(const StateChangeListener &listener, const void* cbParams) = 0;

    virtual void unregisterListener(int tag) = 0;


#if (AI_BUILD_TYPE == AI_DEBUG)

public:
    // Debug interface
    virtual bool createBundle(const std::string& id,
                              const std::string& jsonSpec) const = 0;

    virtual std::string getSpec(int32_t descriptor) const = 0;

    virtual std::string getOCIConfig(int32_t descriptor) const = 0;

#if (AI_ENABLE_TRACING)
    virtual bool startInProcessTracing(int traceFileFd,
                                       const std::string &categoryFilter) const = 0;

    virtual bool stopInProcessTracing() const = 0;
#endif // (AI_ENABLE_TRACING)

#endif // (AI_BUILD_TYPE == AI_DEBUG)

};

#endif // !defined(IDOBBYPROXY_H)
