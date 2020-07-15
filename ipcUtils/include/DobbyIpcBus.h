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
 * File:   DobbyIpcBus.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef DOBBYIPCBUS_H
#define DOBBYIPCBUS_H

#include <IIpcService.h>

#include <map>
#include <deque>
#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>


typedef std::function<void(bool)> ServiceHandlerFn;


// -----------------------------------------------------------------------------
/**
 *  @class DobbyIpcBus
 *  @brief Wraps an IPC service object on a given bus
 *
 *  This class is a helper for the DobbyUtils.  It is used to manage a connection
 *  to a dbus so that plugins don't need to do the heavy lifting.
 *
 *  For example these objects allow for the bus coming and going, and for
 *  managing multiple clients of the bus. It is possible for the bus address
 *  to be changed and the clients won't notice or have to re-register their
 *  handlers.
 *
 *  This class is not intended to replace the IpcService class, in fact it
 *  relies on it quite heavily, rather it is intended to manage that class for
 *  multiple clients.
 *
 */
class DobbyIpcBus
{
public:
    DobbyIpcBus(const std::string& dbusAddress,
                const std::shared_ptr<AI_IPC::IIpcService>& ipcService);
    DobbyIpcBus();
    ~DobbyIpcBus();

public:
    bool connect(const std::string& address);
    void disconnect();

public:
    const std::string& address() const;
    const std::string& socketPath() const;

public:
    std::shared_ptr<AI_IPC::IAsyncReplyGetter> invokeMethod(const AI_IPC::Method& method,
                                                            const AI_IPC::VariantList& args,
                                                            int timeoutMs) const;

    bool invokeMethod(const AI_IPC::Method& method,
                      const AI_IPC::VariantList& args,
                      AI_IPC::VariantList& replyArgs) const;

    bool emitSignal(const AI_IPC::Signal& signal,
                    const AI_IPC::VariantList& args) const;

public:
    bool serviceAvailable(const std::string& serviceName) const;

public:
    int registerServiceHandler(const std::string& serviceName,
                               const ServiceHandlerFn& handlerFunc);

    int registerSignalHandler(const AI_IPC::Signal& signal,
                              const AI_IPC::SignalHandler& handlerFunc);

    void unregisterHandler(int handlerId);


private:
    void disconnectNoLock();

    void registerServiceWatcher();
    void serviceNameChanged(const AI_IPC::VariantList& args);

    void serviceChangeThread();

private:
    static std::string socketPathFromAddress(const std::string& address);


private:
    mutable std::mutex mLock;
    
    std::shared_ptr<AI_IPC::IIpcService> mService;

    std::string mDbusAddress;
    std::string mDbusSocketPath;

    int mHandlerId;

    std::string mServiceSignal;

private:
    typedef struct tagServiceHandler
    {
        tagServiceHandler(const std::string& name_,
                          const ServiceHandlerFn& handler_)
            : name(name_), handler(handler_)
        { }

        std::string name;
        ServiceHandlerFn handler;
    } ServiceHandler;

    std::map<int, ServiceHandler> mServiceHandlers;

    typedef struct tagSignalHandler
    {
        tagSignalHandler(const std::string& regId_,
                         const AI_IPC::Signal& signal_,
                         const AI_IPC::SignalHandler& handler_)
            : regId(regId_), signal(signal_), handler(handler_)
        { }

        std::string regId;
        AI_IPC::Signal signal;
        AI_IPC::SignalHandler handler;
    } SignalHandler;

    std::map<int, SignalHandler> mSignalHandlers;

private:
    std::thread mServiceChangeThread;
    std::mutex mServiceChangeLock;
    std::condition_variable mServiceChangeCond;

    typedef struct tagServiceChangeEvent
    {
        enum EventType { Terminate, ServiceAdded, ServiceRemoved };

        tagServiceChangeEvent(EventType type_)
            : type(type_)
        { }
        tagServiceChangeEvent(EventType type_, const std::string &serviceName_)
            : type(type_)
            , serviceName(serviceName_)
        { }

        const EventType type;
        std::string serviceName;
    } ServiceChangeEvent;

    std::deque<ServiceChangeEvent> mServiceChangeQueue;
};




#endif // !defined(DOBBYIPCBUS_H)
