/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#ifndef AI_IPC_IPCCOMMON_H
#define AI_IPC_IPCCOMMON_H

#include "IpcVariantList.h"
#include <string>
#include <map>
#include <vector>
#include <functional>
#include <memory>
#include <gmock/gmock.h>

namespace AI_IPC
{

class IAsyncReplySenderApiImpl {

public:
    virtual ~IAsyncReplySenderApiImpl() = default;
    virtual bool sendReply(const VariantList& replyArgs) = 0;
    virtual VariantList getMethodCallArguments() const = 0;
};


class IAsyncReplySender {

protected:
   static IAsyncReplySenderApiImpl* impl;

public:

    static void setImpl(IAsyncReplySenderApiImpl* newImpl)
    {
        impl = newImpl;
    }

    static bool sendReply(const VariantList& replyArgs)
    {
        EXPECT_NE(impl, nullptr);

        return impl->sendReply(replyArgs);
    }

    static VariantList getMethodCallArguments()
    {
        EXPECT_NE(impl, nullptr);

        return impl->getMethodCallArguments();
    }

    static IAsyncReplySender* getInstance()
    {
        static IAsyncReplySender* instance = nullptr;
        if(nullptr == instance)
        {
            instance = new IAsyncReplySender();
        }
        return instance;
    }
};


  /**
     * @brief Remote entry, which can be either a signal or method
     *
     * A remote entry is identified by its type, object, interface and name. A remote instance is what we
     * call here as object, which can be accessed though its interfaces.
     *
     * Both methods and signals are part of object interfaces.
     */
    struct RemoteEntry
    {
        enum Type
        {
           METHOD,
           SIGNAL
        };

        Type type;
        std::string service;
        std::string object;
        std::string interface;
        std::string name;

        bool isValid() const
        {
            if (object.empty() || interface.empty() || name.empty())
                return false;
            if ((type == METHOD) && service.empty())
                return false;
            return true;
        }

        operator bool() const
        {
            return isValid();
        }

    protected:
        RemoteEntry(Type type_)
            : type(type_) {}

        RemoteEntry(Type type_, const std::string& service_, const std::string& object_, const std::string& interface_, const std::string& name_)
            : type(type_), service(service_), object(object_), interface(interface_), name(name_) {}
    };

    /**
     * @brief Method identified by a service, object, interface and method name itself
     */
    struct Method : public RemoteEntry
    {
        Method()
            : RemoteEntry(METHOD) {}

        Method(const std::string& service_, const std::string& object_, const std::string& interface_, const std::string& name_)
            : RemoteEntry(METHOD, service_, object_, interface_, name_) {}

    };

    /**
     * @brief Method identified by object, interface and signal name itself
     *
     * See dbus signal for details
     */
    struct Signal : public RemoteEntry
    {
        Signal()
            : RemoteEntry(SIGNAL) {}

        Signal(const std::string& object_, const std::string& interface_, const std::string& name_)
            : RemoteEntry(SIGNAL, std::string(), object_, interface_, name_) {}
    };

class IAsyncReplyGetter {
public:
    virtual ~IAsyncReplyGetter() = default;

    /**
     * @brief Get reply.
     *
     * @parameter[out]   argList         This is the return value of the method call
     *
     * @returns On success: TRUE
     * @returns On failure: FALSE
     */
    virtual bool getReply(VariantList& argList) = 0;
};



/**
 * @brief dbus monitor event types.
 */
typedef enum
{
    MethodCallEvent,
    MethodReturnEvent,
    SignalEvent,
    ErrorEvent,
} EventType;


/**
 * @brief Method call handler
 */
typedef std::function<void(std::shared_ptr<IAsyncReplySender>)> MethodHandler;

/**
 * @brief Signal handler
 */
typedef std::function<void(const VariantList&)> SignalHandler;

/**
 * @brief Monitor handler
 */
typedef std::function<void(EventType,
                           unsigned int,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const std::string&,
                           const VariantList&)> MonitorHandler;


constexpr auto sendReply = &IAsyncReplySender::sendReply;


} // namespace AI_IPC

#endif /* AI_IPC_IPCCOMMON_H */
