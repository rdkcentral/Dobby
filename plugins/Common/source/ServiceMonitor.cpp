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
 * File:   ServiceMonitor.cpp
 *
 * Copyright (C) Sky UK 2016+
 */
#include "ServiceMonitor.h"

#include <Logging.h>



ServiceMonitor::ServiceMonitor(const std::shared_ptr<IDobbyIPCUtils>& ipcUtils,
                               const std::shared_ptr<IDobbyUtils>& utils,
                               const IDobbyIPCUtils::BusType& bus,
                               const std::string& serviceName,
                               const AI_IPC::Signal& readySignal,
                               const AI_IPC::Method& queryMethod,
                               const std::function<void(State)>& stateChangeHandler)
    : mUtilities(utils)
    , mIPCUtilities(ipcUtils)
    , mBusType(bus)
    , mServiceName(serviceName)
    , mReadySignal(readySignal)
    , mQueryMethod(queryMethod)
    , mStateChangeHandler(stateChangeHandler)
    , mState(State::NotRunning)
    , mServiceHandlerId(-1)
    , mSignalHandlerId(-1)
    , mTimerId(-1)
{
    AI_LOG_FN_ENTRY();

    // setup the ipc connection monitor, we want to know if the service falls
    // off the bus
    std::function<void(bool)> notifyFn = std::bind(&ServiceMonitor::onServiceNotification,
                                                   this, std::placeholders::_1);
    mServiceHandlerId = mIPCUtilities->ipcRegisterServiceHandler(bus, serviceName, notifyFn);


    // and install a listener for when the service tells us it's 'ready'
    const AI_IPC::SignalHandler signalFn = std::bind(&ServiceMonitor::onReadyNotification,
                                                     this, std::placeholders::_1);
    mSignalHandlerId = mIPCUtilities->ipcRegisterSignalHandler(bus, readySignal, signalFn);



    // for extra belts and braces we add a period timer that runs every second
    // and sends out a ping request to the service daemon if not already in
    // the ready state
    const std::function<bool()> timerFn = std::bind(&ServiceMonitor::onTimer, this);
    mTimerId = mUtilities->startTimer(std::chrono::seconds(1), false, timerFn);


    // and finally check if the service is currently available, typically the
    // service is not expected to be available when the plugin is first loaded
    if (mIPCUtilities->ipcServiceAvailable(bus, serviceName))
    {
        // the service is running
        mState = State::Running;

        // so send a request for it's current status
        sendIsReadyRequest();
    }

    AI_LOG_FN_EXIT();
}

ServiceMonitor::~ServiceMonitor()
{
    AI_LOG_FN_ENTRY();

    // can cancel the periodic ping timer
    if (mTimerId >= 0)
    {
        mUtilities->cancelTimer(mTimerId);
        mTimerId = -1;
    }

    // we no longer care about the jumper daemon's state
    if (mServiceHandlerId >= 0)
    {
        mIPCUtilities->ipcUnregisterHandler(mBusType, mServiceHandlerId);
        mServiceHandlerId = -1;
    }
    if (mSignalHandlerId >= 0)
    {
        mIPCUtilities->ipcUnregisterHandler(mBusType, mSignalHandlerId);
        mSignalHandlerId = -1;
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the current state of the service
 *
 *
 */
ServiceMonitor::State ServiceMonitor::state() const
{
    std::lock_guard<std::mutex> locker(mLock);
    return mState;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends out an 'is ready' method request to the service
 *
 *
 */
void ServiceMonitor::forceReadyCheck()
{
    sendIsReadyRequest();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback function called when the daemon service has been added or
 *  removed from the bus.
 *
 *  We use this point to adjust the @a mState state.
 *
 *
 *
 *  @param[in]  added       Will be true if the service arrived on the bus,
 *                          false if it has left the bus.
 *
 */
void ServiceMonitor::onServiceNotification(bool added)
{
    AI_LOG_INFO("%s service %s", mServiceName.c_str(), added ? "added" : "removed");

    std::unique_lock<std::mutex> locker(mLock);

    State newState = mState;

    if (added)
    {
        // move our internal state to running, this is not the same as 'ready'
        // which we expect to receive shortly
        if (mState == State::NotRunning)
            newState = State::Running;
    }
    else
    {
        newState = State::NotRunning;
    }

    // call the registered handler
    if (newState != mState)
    {
        mState = newState;

        locker.unlock();

        if (mStateChangeHandler)
            mStateChangeHandler(newState);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Callback function called when the service daemon has sent a signal
 *  saying it's ready to process requests
 *
 *  We use this point to adjust the @a mState state.
 *
 *
 *  @param[in]  args        Arguments supplied with the signal (ignored).
 *
 */
void ServiceMonitor::onReadyNotification(const AI_IPC::VariantList& args)
{
    AI_LOG_INFO("%s service is ready", mServiceName.c_str());

    std::unique_lock<std::mutex> locker(mLock);

    if (mState != State::Ready)
    {
        // set the state back to ready
        mState = State::Ready;

        locker.unlock();

        // call the registered handler
        if (mStateChangeHandler)
            mStateChangeHandler(State::Ready);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Timer handler called every second, it sends out a ping request to
 *  the daemon if we think it is not alive
 *
 *
 */
bool ServiceMonitor::onTimer()
{
    // take the lock and check if we think the daemon isn't there
    std::unique_lock<std::mutex> locker(mLock);

    if (mState != State::Ready)
    {
        locker.unlock();

        sendIsReadyRequest();
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends a method call over dbus asking the service daemon to reply with
 *  a ready notification / signal if it is actually there
 *
 *
 */
void ServiceMonitor::sendIsReadyRequest() const
{
    AI_LOG_FN_ENTRY();

    // send a method call to the daemon asking it to send an 'is ready' signal
    // if it is in fact ready ... we are not expecting a reply to this method
    // so we throw the async reply getter away
    std::shared_ptr<AI_IPC::IAsyncReplyGetter> reply;
    reply = mIPCUtilities->ipcInvokeMethod(mBusType, mQueryMethod, { });

    if (!reply)
    {
        AI_LOG_ERROR("failed to invoke method '%s'",
                     mQueryMethod.name.c_str());
    }

    // there is no reply to this method, instead if the daemon is alive it will
    // broadcast a 'is ready' message

    AI_LOG_FN_EXIT();
}

