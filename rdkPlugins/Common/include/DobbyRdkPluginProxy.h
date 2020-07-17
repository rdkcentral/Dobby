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

#ifndef DOBBYRDKPLUGINPROXY_H
#define DOBBYRDKPLUGINPROXY_H


#include <IIpcService.h>
#include <IDGenerator.h>

#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <list>
#include <pthread.h>

// -----------------------------------------------------------------------------
/**
 *  @class DobbyRdkPluginProxy
 *  @brief Wrapper around an IpcService object that provides simple method
 *  calls to the dobby daemon.
 *
 */
class DobbyRdkPluginProxy
{
public:
    DobbyRdkPluginProxy(const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
                        const std::string& serviceName,
                        const std::string& objectName);
    ~DobbyRdkPluginProxy();

public:
    int32_t getBridgeConnections() const;
    uint32_t getIpAddress(const std::string &vethName) const;
    bool freeIpAddress(uint32_t address) const;
    std::vector<std::string> getExternalInterfaces() const;

private:
    bool invokeMethod(const char *interface_, const char *method_,
                      const AI_IPC::VariantList& params_,
                      AI_IPC::VariantList& returns_) const;

private:
    const std::shared_ptr<AI_IPC::IIpcService> mIpcService;
    const std::string mServiceName;
    const std::string mObjectName;

};

#endif // !defined(DOBBYRDKPLUGINPROXY_H)
