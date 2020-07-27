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
 * File:   ServiceMonitor.h
 *
 */
#ifndef SERVICEMONITOR_H
#define SERVICEMONITOR_H

#include <IDobbyIPCUtils.h>
#include <IDobbyUtils.h>

#include <mutex>
#include <memory>

// -----------------------------------------------------------------------------
/**
 *  @class ServiceMonitor
 *  @brief Utility class to actively monitor the status of a dbus service
 *
 *  Previously this was implemented separately in various plugins, however the
 *  process is generic enough that it could be implemented in one object.
 *
 *  The class has the following requirements on the dbus service:
 *      a) The dbus service must emit a signal when it is ready
 *      b) The dbus service must implement a method to trigger the signal in a)
 *         on request.
 *
 *  You can think of a) as a pong message, and b) the ping.  The method in b)
 *  should not return a value in the method call, instead it should trigger
 *  a signal a) if ready.
 *
 *  The class internally implements a timer on a one second period, it will
 *  send our feeler requests to see if the service is ready if not already in
 *  the ready state.
 *
 */
class ServiceMonitor
{
public:
    enum class State
    {
        NotRunning, ///< dbus service not detected on the bus
        Running,    ///< dbus service is detected but haven't received
                    ///  the 'ready' signal
        Ready       ///< ready signal received
    };

public:
    ServiceMonitor(const std::shared_ptr<IDobbyIPCUtils> &ipcUtils,
                   const std::shared_ptr<IDobbyUtils> &utils,
                   const IDobbyIPCUtils::BusType &bus,
                   const std::string &serviceName,
                   const AI_IPC::Signal &readySignal,
                   const AI_IPC::Method &queryMethod,
                   const std::function<void(State)> &stateChangeHandler);
    ~ServiceMonitor();

public:
    State state() const;

    void forceReadyCheck();

private:
    void onServiceNotification(bool added);
    void onReadyNotification(const AI_IPC::VariantList &args);

    bool onTimer();

    void sendIsReadyRequest() const;

private:
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::shared_ptr<IDobbyIPCUtils> mIPCUtilities;
    const IDobbyIPCUtils::BusType mBusType;
    const std::string mServiceName;
    const AI_IPC::Signal mReadySignal;
    const AI_IPC::Method mQueryMethod;
    const std::function<void(State)> mStateChangeHandler;

private:
    mutable std::mutex mLock;
    State mState;

    int mServiceHandlerId;
    int mSignalHandlerId;
    int mTimerId;
};

#endif // !defined(SERVICEMONITOR_H)
