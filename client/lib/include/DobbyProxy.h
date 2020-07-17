/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * DobbyProxy.h
 * Author:
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef DOBBYPROXY_H
#define DOBBYPROXY_H

#include <Dobby/IDobbyProxy.h>

#include <IIpcService.h>
#include <IDGenerator.h>

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>


// -----------------------------------------------------------------------------
/**
 *  @class DobbyProxy
 *  @brief Wrapper around an IpcService object that provides simplier method
 *  calls to the dobby daemon.
 *
 *
 *
 *
 */
class DobbyProxy : public IDobbyProxy
{
public:
    DobbyProxy(const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
               const std::string& serviceName,
               const std::string& objectName);
    ~DobbyProxy() final;

public:
    // Admin interface
    bool shutdown() const override;

    bool ping() const override;

    bool isAlive(const std::chrono::milliseconds& timeout) const override;

    bool setLogMethod(uint32_t method, int pipeFd) const override;

    bool setLogLevel(int level) const override;

    bool setAIDbusAddress(bool privateBus,
                          const std::string& address) const override;

public:
    // Control interface
    int32_t startContainerFromSpec(const std::string& id,
                                   const std::string& jsonSpec,
                                   const std::list<int>& files,
                                    const std::string& command = "") const override;

    int32_t startContainerFromSpec(const std::string& id,
                                   const Json::Value& spec,
                                   const std::list<int>& files,
                                   const std::string& command = "") const override;


    int32_t startContainerFromBundle(const std::string& id,
                                     const std::string& bundlePath,
                                     const std::list<int>& files,
                                     const std::string& command = "") const override;

    bool stopContainer(int32_t cd, bool withPrejudice) const override;

    bool pauseContainer(int32_t cd) const override;

    bool resumeContainer(int32_t cd) const override;

    bool execInContainer(int32_t cd,
                         const std::string& options,
                         const std::string& command) const override;

    int getContainerState(int32_t cd) const override;

    int registerListener(const StateChangeListener &listener, const void* cbParams) override;

    void unregisterListener(int tag) override;

    std::string getContainerInfo(int32_t descriptor) const override;

    std::list<std::pair<int32_t, std::string>> listContainers() const override;

#if (AI_BUILD_TYPE == AI_DEBUG)

public:
    // Debug interface
    bool createBundle(const std::string& id,
                      const std::string& jsonSpec) const override;

    std::string getSpec(int32_t descriptor) const override;

    std::string getOCIConfig(int32_t descriptor) const override;

#endif // (AI_BUILD_TYPE == AI_DEBUG)

private:
    void onContainerStartedEvent(const AI_IPC::VariantList& args);
    void onContainerStoppedEvent(const AI_IPC::VariantList& args);

private:
    bool invokeMethod(const char *interface_, const char *method_,
                      const AI_IPC::VariantList& params_,
                      AI_IPC::VariantList& returns_) const;

    void containerStateChangeThread();

private:
    const std::shared_ptr<AI_IPC::IIpcService> mIpcService;
    const std::string mServiceName;
    const std::string mObjectName;

private:
    std::string mContainerStartedSignal;
    std::string mContainerStoppedSignal;

private:
    std::thread mStateChangeThread;
    std::mutex mStateChangeLock;
    std::condition_variable mStateChangeCond;

    struct StateChangeEvent
    {
        enum Type { Terminate, ContainerStarted, ContainerStopped };

        explicit StateChangeEvent(Type type_)
            : type(type_), descriptor(-1)
        { }

        StateChangeEvent(Type type_, int32_t descriptor_, const std::string& name_)
            : type(type_), descriptor(descriptor_), name(name_)
        { }

        Type type;
        int32_t descriptor;
        std::string name;
    };

    std::deque<StateChangeEvent> mStateChangeQueue;

    std::mutex mListenersLock;
    AICommon::IDGenerator<8> mListenerIdGen;
    std::map<int, std::pair<StateChangeListener, const void*>> mListeners;

};



#endif // !defined(DOBBYPROXY_H)
