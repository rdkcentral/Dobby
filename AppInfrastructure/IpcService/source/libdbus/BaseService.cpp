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
 * File: BaseService.cpp
 *
 * Copyright (C) Sky 2018+
 */
#include "BaseService.h"
#include <Logging.h>

AI_IPC::BaseService::BaseService(const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
                                 const std::string &serviceName,
                                 const std::string &serviceObject)
    : mIpcService(ipcService)
    , mServiceName(serviceName)
    , mServiceObject(serviceObject)
{

}

AI_IPC::BaseService::~BaseService()
{
    AI_LOG_FN_ENTRY();

    // unregister all the method handlers
    for (const std::string& handlerId : mMethodHandlers)
    {
        if (!mIpcService->unregisterHandler(handlerId))
        {
            AI_LOG_ERROR( "failed to unregister '%s'", handlerId.c_str());
        }
    }
    mMethodHandlers.clear();

    // flush the dbus event queue
    mIpcService->flush();

    AI_LOG_FN_EXIT();
}

void AI_IPC::BaseService::registerServiceMethods(const std::vector<AI_IPC::BaseService::ServiceMethod> &methods)
{
    for(const ServiceMethod& method: methods)
    {
        std::string methodId =
                mIpcService->registerMethodHandler(AI_IPC::Method(mServiceName, mServiceObject, method.mIface, method.mName), method.mFunc);
        if (methodId.empty())
        {
            AI_LOG_ERROR("failed to register '%s' method", method.mName);
        }
        else
        {
            mMethodHandlers.push_back(methodId);
        }
    }
}
