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
 * DobbyFactory.cpp
 * Author:
 *
 * Copyright (C) BSKYB 2016+
 */
#include "DobbyFactory.h"
#include "DobbyProxy.h"
#include <DobbyProtocol.h>

#include <Dobby/IDobbyProxy.h>

#include "Upstart.h"

#include <Mutex.h>
#include <ConditionVariable.h>
#include <IIpcService.h>
#include <Logging.h>

#if !defined(RDK)
#   include <app-logger.h>
#endif

#include <chrono>

DobbyFactory::DobbyFactory(const std::shared_ptr<AI_IPC::IIpcService> &ipcService)
    : mIpcService(ipcService)
{
}

DobbyFactory::~DobbyFactory()
{
    AI_LOG_FN_ENTRY();

    mProxy.reset();
    mIpcService.reset();

    AI_LOG_FN_EXIT();
}

void DobbyFactory::setWorkspacePath(const std::string& path)
{
    std::lock_guard<std::mutex> locker(mLock);
    mWorkspacePath = path;
}

void DobbyFactory::setFlashMountPath(const std::string& path)
{
    std::lock_guard<std::mutex> locker(mLock);
    mFlashMountPath = path;
}

void DobbyFactory::setPlatformIdent(const std::string& platformIdent)
{
    std::lock_guard<std::mutex> locker(mLock);
    mPlatformIdent = platformIdent;
}

void DobbyFactory::setPlatformType(const std::string& platformType)
{
    std::lock_guard<std::mutex> locker(mLock);
    mPlatformType = platformType;
}

void DobbyFactory::setPlatformModel(const std::string& platformModel)
{
    std::lock_guard<std::mutex> locker(mLock);
    mPlatformModel = platformModel;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sends pings to the Dobby 'Hypervisor' daemon until a pong is received
 *  or we timeout.
 *
 *  This sends ping method calls to the daemon and waits for a second for a
 *  reply, we do this for 60 seconds before giving up.
 *
 *  This method shouldn't be required, but we've seen timeouts at start-up and
 *  this is an attempt to recover.
 *
 *  @return true if a ping / pong was received, false on failure.
 */
bool DobbyFactory::pingDobbyDaemon()
{
    // calculate the timeout time (60 seconds) before giving up
    std::chrono::time_point<std::chrono::steady_clock> timeoutAfter =
        (std::chrono::steady_clock::now() + std::chrono::seconds(60));

    // construct the ping method call
    const AI_IPC::Method pingMethod(DOBBY_SERVICE,
                                    DOBBY_OBJECT,
                                    DOBBY_ADMIN_INTERFACE,
                                    DOBBY_ADMIN_METHOD_PING);

    // poll until ping returns success or we timeout
    AI_IPC::VariantList pingReply;
    while (!mIpcService->invokeMethod(pingMethod, { }, pingReply, 1000))
    {
        if (std::chrono::steady_clock::now() > timeoutAfter)
        {
            AI_LOG_ERROR_EXIT("timed-out waiting for a ping to be responded "
                              "to from Dobby daemon");
            return false;
        }
    }

    AI_LOG_INFO("received pong message from daemon");

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts the Dobby 'Hypervisor' daemon
 *
 *  The function tries to the start the daemon using Upstart, if this fails
 *  we're in trouble and just returns false.
 *
 *
 *  @return true on success, false on failure.
 */
bool DobbyFactory::startDobbyDaemon()
{
    AI_LOG_FN_ENTRY();

    // setup a signal handler to catch the 'ready' signal from the daemon
    volatile bool ready = false;
    AICommon::Mutex readyMutex;
    AICommon::ConditionVariable readyCondVar;

    // this is a lambda function, and they're ugly, however in this particular
    // instance it's useful because the signal handler only needs to signal
    // the conditional
    const AI_IPC::SignalHandler handler = [&](const AI_IPC::VariantList& args)
    {
        AI_LOG_INFO("received 'ready' signal from DobbyDaemon");

        std::unique_lock<AICommon::Mutex> locker(readyMutex);
        ready = true;

        readyCondVar.notify_all();
    };

    // register the signal handler
    const AI_IPC::Signal signal(DOBBY_OBJECT, DOBBY_ADMIN_INTERFACE, DOBBY_ADMIN_EVENT_READY);
    const std::string id = mIpcService->registerSignalHandler(signal, handler);
    if (id.empty())
    {
        AI_LOG_ERROR_EXIT("failed to register signal handler");
        return false;
    }



    // create an Upstart object to talk to the upstart daemon
    Upstart upstart(mIpcService);

    // create a set of environment variables for the Dobby daemon
    const std::vector<std::string> envs = {
        "AI_WORKSPACE_PATH="  + mWorkspacePath,
        "AI_PERSISTENT_PATH=" + mFlashMountPath,
        "AI_PLATFORM_IDENT="  + mPlatformIdent,
        "AI_PLATFORM_TYPE="   + mPlatformType,
        "AI_PLATFORM_MODEL="  + mPlatformModel
    };

    AI_LOG_MILESTONE("attempting to start Dobby hypervisor");

    // try and start the Dobby daemon; the 'skyDobbyDaemon' string is the name
    // of the config file stored here /etc/init/skyDobbyDaemon.conf. This conf
    // file is copied into the rootfs by the makefiles in the Dobby component
    if (!upstart.start("skyDobbyDaemon", envs))
    {
        mIpcService->unregisterHandler(id);
        mIpcService->flush();

        AI_LOG_FATAL_EXIT("failed to start the Dobby 'Hypervisor' daemon, "
                          "this really is fatal");
        return false;
    }

    // wait for the ready signal
    bool success = false;
    {
        std::unique_lock<AICommon::Mutex> locker(readyMutex);
        while (!ready)
        {
            if (readyCondVar.wait_for(locker, std::chrono::seconds(5)) == std::cv_status::timeout)
            {
                AI_LOG_ERROR("timed-out waiting for the ready signal from the "
                             "daemon, falling back to ping polling");
                break;
            }
        }

        success = ready;
    }

    // unregister the signal handler, we only want to be called once
    mIpcService->unregisterHandler(id);
    mIpcService->flush();

    // for NGDEV-67175 we fallback to sending ping / pong messages to the daemon
    // for a while until finally giving up if we get no response
    if (!success)
    {
        success = pingDobbyDaemon();
    }

    AI_LOG_FN_EXIT();
    return success;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts the Dobby 'Hypervisor' daemon and returns a proxy object
 *
 *  The function first tries to the start the daemon using Upstart, if this
 *  fails we're in trouble and just returns an invalid proxy.
 *
 *  If the daemon did start then we try and open a connection to it, if that
 *  was successful we wrap it in a proxy object and return that.  The proxy
 *  is just a lightweight wrapper around the dbus method calls into the
 *  daemon.
 *
 *  @return a shared_ptr to an IDobbyProxy object which allows you to start /
 *  stop containers, etc.  On failure an empty shared_ptr is returned.
 */
std::shared_ptr<IDobbyProxy> DobbyFactory::getProxy()
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // sanity check the paths and platform string have been set
    if (mWorkspacePath.empty() || mFlashMountPath.empty() || mPlatformIdent.empty())
    {
        AI_LOG_FATAL_EXIT("one or more of the path / platform strings haven't been set");
        return nullptr;
    }

    // sanity check we have a valid IPC service
    if (!mIpcService)
    {
        AI_LOG_FATAL_EXIT("missing valid IpcService object");
        return nullptr;
    }

    // create the proxy object for for the daemon if we haven't already
    if (!mProxy)
    {
        // start up the dobby daemon (if not already)
        if (!startDobbyDaemon())
        {
            return nullptr;
        }

        // create a proxy connection to the daemon
        mProxy = std::make_shared<DobbyProxy>(mIpcService, DOBBY_SERVICE, DOBBY_OBJECT);
        if (!mProxy)
        {
            AI_LOG_ERROR("failed to create proxy shared_ptr");
        }

#if !defined(RDK)

        // the final step is to gift a logging pipe to the daemon
        unsigned int loggingLevels = 0;
#if (AI_BUILD_TYPE == AI_DEBUG)
        loggingLevels |= (APPLOG_LEVEL_FATAL_MASK | APPLOG_LEVEL_ERROR_MASK |
                          APPLOG_LEVEL_WARNING_MASK | APPLOG_LEVEL_MILESTONE_MASK |
                          APPLOG_LEVEL_INFO_MASK | APPLOG_LEVEL_DEBUG_MASK);
#endif

        int pipeFd = APPLOG_CreateClientPipe("DOBBY", loggingLevels, -1);
        if (pipeFd < 0)
        {
            AI_LOG_ERROR("failed to create logging pipe");
        }
        else
        {
            mProxy->setLogMethod(DOBBY_LOG_ETHANLOG, pipeFd);

            if (close(pipeFd) != 0)
                AI_LOG_SYS_ERROR(errno, "failed to close app pipe fd");
        }

#endif // !defined(RDK)
    }

    AI_LOG_FN_EXIT();
    return mProxy;
}
