/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2018 Sky UK
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
 * File: BaseService.h
 *
 */
#ifndef BASESERVICE_H
#define BASESERVICE_H

#include <IIpcService.h>
#include <list>
#include <string>
#include <functional>

namespace AI_IPC
{

/**
 * @brief The BaseService class this is a base class for DBUS services. It has some helper
 * functions to make it easier to register callbacks and do some cleanup when deinitialising.
 */
class BaseService
{
public:
    BaseService(const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
                const std::string &serviceName,
                const std::string &serviceObject);
    virtual ~BaseService();
protected:
    struct ServiceMethod
    {
        ServiceMethod(const char *iface, const char* name, std::function<void(std::shared_ptr<AI_IPC::IAsyncReplySender>)> func) :
            mIface(iface), mName(name), mFunc(func) {}
        const char *mIface;
        const char *mName;
        std::function<void(std::shared_ptr<AI_IPC::IAsyncReplySender>)> mFunc;
    };

    void registerServiceMethods(const std::vector<ServiceMethod>& methods);
private:
    const std::shared_ptr<AI_IPC::IIpcService> mIpcService;
    const std::string mServiceName;
    const std::string mServiceObject;
    std::list<std::string> mMethodHandlers;
};

} // namespace AI_IPC

#endif // BASESERVICE_H
