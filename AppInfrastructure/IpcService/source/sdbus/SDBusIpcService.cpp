/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
//
//  SDBusIpcService.cpp
//  IpcService
//
//

#include "SDBusIpcService.h"
#include "SDBusAsyncReplyGetter.h"
#include "SDBusAsyncReplySender.h"
#include "SDBusArguments.h"

#include <Logging.h>

#include <systemd/sd-bus.h>
#include <systemd/sd-event.h>

#include <sstream>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/eventfd.h>


using namespace AI_IPC;



SDBusIpcService::SDBusIpcService(const std::string &busAddress,
                                 const std::string& serviceName,
                                 int defaultTimeoutMs)
    : mDefaultTimeoutUsecs(25 * 1000 * 1000)
    , mSDBus(nullptr)
    , mStarted(false)
    , mHandlerTag(1)
    , mExecCounter(1)
    , mLastExecTag(0)
    , mExecEventFd(-1)
{

    // create a new bus, set it's address then open it
    sd_bus *bus = nullptr;
    int rc = sd_bus_new(&bus);
    if ((rc < 0) || !bus)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to create sd-bus object");
        return;
    }

    rc = sd_bus_set_address(bus, busAddress.c_str());
    if (rc < 0)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to create sd-bus object");
        return;
    }

    // set some boilerplate stuff for the connection
    sd_bus_set_bus_client(bus, true);
    sd_bus_set_trusted(bus, false);
    sd_bus_negotiate_creds(bus, true, (SD_BUS_CREDS_UID | SD_BUS_CREDS_EUID |
                                       SD_BUS_CREDS_EFFECTIVE_CAPS));

    // start the bus
    rc = sd_bus_start(bus);
    if (rc < 0)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to start the bus");
        return;
    }

    // store the open connection
    mSDBus = bus;

    // initialise the reset and register ourselves on the bus
    if (!init(serviceName, defaultTimeoutMs))
    {
        AI_LOG_FATAL("failed to init object");
        return;
    }
}


SDBusIpcService::SDBusIpcService(BusType busType,
                                 const std::string& serviceName,
                                 int defaultTimeoutMs)
    : mDefaultTimeoutUsecs(25 * 1000 * 1000)
    , mSDBus(nullptr)
    , mStarted(false)
    , mHandlerTag(1)
    , mExecCounter(1)
    , mLastExecTag(0)
    , mExecEventFd(-1)
{
    // populate reply identifier pool with identifiers
    for (int i = 1; i < 50; i++)
    {
        mReplyIdentifiers.push(i);
    }

    // open dbus
    sd_bus *bus = nullptr;
    int rc = -1;
    switch (busType)
    {
        case SystemBus:
            rc = sd_bus_open_system(&bus);
            break;
        case SessionBus:
            rc = sd_bus_open_user(&bus);
            break;
    }

    if ((rc < 0) || !bus)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to open connection to dbus");
        return;
    }

    // store the open connection
    mSDBus = bus;

    // initialise the reset and register ourselves on the bus
    if (!init(serviceName, defaultTimeoutMs))
    {
        AI_LOG_FATAL("failed to init object");
        return;
    }
}

SDBusIpcService::~SDBusIpcService()
{
    // ensure the event loop is stopped
    if (mThread.joinable())
    {
        // lambda to quit the event loop
        std::function<void()> quitExec =
            []()
            {
                // get the event loop object
                sd_event *loop = nullptr;
                int rc = sd_event_default(&loop);
                if ((rc < 0) || (loop == nullptr))
                {
                    AI_LOG_SYS_FATAL(-rc, "failed to get event loop pointer");
                    return;
                }

                // ask the event loop to exit
                sd_event_exit(loop, 0);

                // free the local reference
                sd_event_unref(loop);
            };

        // invoke the quit lambda
        runOnEventLoopThread(std::move(quitExec));

        // wait for the thread to quit
        mThread.join();
    }

    // close and free the sd-bus
    if (mSDBus)
    {
        sd_bus_close(mSDBus);
        sd_bus_unref(mSDBus);
        mSDBus = nullptr;
    }

    // close the fd used to signal the event loop
    if ((mExecEventFd >= 0) && (close(mExecEventFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close eventfd");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal


 */
bool SDBusIpcService::init(const std::string &serviceName,
                           int defaultTimeoutMs)
{
    // set the default timeout value in microseconds
    if (defaultTimeoutMs <= 0)
        mDefaultTimeoutUsecs = (25 * 1000 * 1000);
    else
        mDefaultTimeoutUsecs = (defaultTimeoutMs * 1000);


    // eventfd used to wake the poll loop
    mExecEventFd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (mExecEventFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to created eventfd");
        return false;
    }

    // register our service name
    if (!serviceName.empty())
    {
        int rc = sd_bus_request_name(mSDBus, serviceName.c_str(), 0);
        if (rc < 0)
        {
            AI_LOG_SYS_ERROR(-rc, "failed to register service name '%s' on bus",
                             serviceName.c_str());
        }
    }

    // spawn the event loop thread
    mThread = std::thread(&SDBusIpcService::eventLoopThread, this);
    return mThread.joinable();
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
bool SDBusIpcService::enableMonitor(const std::set<std::string> &matchRules,
                                    const AI_IPC::MonitorHandler& handler)
{
    AI_LOG_ERROR("TODO: monitoring is not yet supported on sd-bus IpcService");
    return false;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
bool SDBusIpcService::disableMonitor()
{
    AI_LOG_ERROR("TODO: monitoring is not yet supported on sd-bus IpcService");
    return false;
}

// -----------------------------------------------------------------------------
/*!
    \overload

    This is a NOP for the SDBus implementation.

 */
void SDBusIpcService::flush()
{
}

// -----------------------------------------------------------------------------
/*!
    \overload

    Starts the IpcService, this is a bit of NOP for sd-bus, it just sets a
    flag to say that handlers will now be called.

 */
bool SDBusIpcService::start()
{
    // sanity check we have a bus
    if (!mSDBus)
    {
        AI_LOG_ERROR("no valid sd-bus object");
        return false;
    }

    // sanity check the event loop thread is running
    if (!mThread.joinable())
    {
        AI_LOG_ERROR("IpcService thread not running");
        return false;
    }

    mStarted = true;
    return true;
}

// -----------------------------------------------------------------------------
/*!
    \overload

    Stops the IpcService by terminating the event loop thread.

 */
bool SDBusIpcService::stop()
{
    // terminate the event loop thread
    if (!mStarted)
    {
        AI_LOG_ERROR("IpcService not started");
        return false;
    }

    // toggle the started flag and then ...
    mStarted = false;

    // add a null event to the event loop to ensure all callbacks queued are
    // processed
    std::function<void()> nopExec =
        []()
        {
            // do nothing
        };

    runOnEventLoopThread(std::move(nopExec));

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \overload

    Checks if the service with the given name \a serviceName is registered on
    the bus and therefore available.

 */
bool SDBusIpcService::isServiceAvailable(const std::string &serviceName) const
{
    // lambda to run in the context of the event loop thread
    bool isRegistered = false;
    std::function<void()> serviceCheckLambda =
        [&]()
        {
            // construct the signal message
            int rc = sd_bus_get_name_creds(mSDBus, serviceName.c_str(), 0, nullptr);
            if (rc < 0)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to get creds for service '%s'",
                                 serviceName.c_str());
                return;
            }

            // if no error then the service has an owner and therefore is
            // registered
            isRegistered = true;
        };

    // run the lambda to emit the signal
    if (!runOnEventLoopThread(std::move(serviceCheckLambda)))
    {
        AI_LOG_ERROR("failed to execute function to emit signals");
        return false;
    }

    // return the result
    return isRegistered;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
std::string SDBusIpcService::getBusAddress() const
{
    const char *address = nullptr;

    int rc = sd_bus_get_address(mSDBus, &address);
    if ((rc < 0) || !address)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to get bus address");
        return std::string();
    }

    return std::string(address);
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
std::shared_ptr<IAsyncReplyGetter> SDBusIpcService::invokeMethod(const Method &method,
                                                                 const VariantList &args,
                                                                 int timeoutMs)
{
    // calculate the timeout for the call in microseconds
    uint64_t timeoutUsecs;
    if (timeoutMs < 0)
        timeoutUsecs = mDefaultTimeoutUsecs;
    else
        timeoutUsecs = (timeoutMs * 1000);

    // create the reply getter
    std::shared_ptr<SDBusAsyncReplyGetter> replyGetter =
        std::make_shared<SDBusAsyncReplyGetter>();

    // lambda to run in the context of the event loop thread
    std::function<void()> methodCallLambda =
        [&]()
        {
            // construct the signal message
            sd_bus_message *msg = nullptr;
            int rc = sd_bus_message_new_method_call(mSDBus, &msg,
                                                    method.service.c_str(),
                                                    method.object.c_str(),
                                                    method.interface.c_str(),
                                                    method.name.c_str());
            if ((rc < 0) || !msg)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to create new method call message");
                replyGetter.reset();
                return;
            }

            // marshall the arguments into it
            SDBusArguments::marshallArgs(msg, args);

            // and fire it off
            rc = sd_bus_call_async(mSDBus, nullptr, msg,
                                   (sd_bus_message_handler_t)&SDBusIpcService::onMethodReply,
                                   this, timeoutUsecs);

            // get the cookie of the sent message
            uint64_t cookie;
            sd_bus_message_get_cookie(msg, &cookie);

            // free the message
            sd_bus_message_unref(msg);

            // and check the request succeeded
            if (rc < 0)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to send method call message");
                replyGetter.reset();
                return;
            }

            // add the async result object to the map against the cookie
            mCalls.emplace(cookie, replyGetter);
        };

    // run the lambda to emit the signal
    if (!runOnEventLoopThread(std::move(methodCallLambda)))
    {
        AI_LOG_ERROR("failed to execute function to call method");
        return nullptr;
    }

    // return the result
    return replyGetter;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
bool SDBusIpcService::invokeMethod(const Method &method,
                                   const VariantList &args,
                                   VariantList &replyArgs,
                                   int timeoutMs)
{
    // calculate the timeout for the call in microseconds
    uint64_t timeoutUsecs;
    if (timeoutMs < 0)
        timeoutUsecs = mDefaultTimeoutUsecs;
    else
        timeoutUsecs = (timeoutMs * 1000);

    // clear the reply args list
    replyArgs.clear();

    // lambda to run in the context of the event loop thread
    bool success = false;
    std::function<void()> methodCallLambda =
        [&]()
        {
            // construct the signal message
            sd_bus_message *msg = nullptr;
            int rc = sd_bus_message_new_method_call(mSDBus, &msg,
                                                    method.service.c_str(),
                                                    method.object.c_str(),
                                                    method.interface.c_str(),
                                                    method.name.c_str());
            if ((rc < 0) || !msg)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to create new method call message");
                return;
            }

            // marshall the arguments into it
            SDBusArguments::marshallArgs(msg, args);

            // and fire it off
            sd_bus_error error = { nullptr, nullptr, false };
            sd_bus_message *reply = nullptr;
            rc = sd_bus_call(mSDBus, msg, timeoutUsecs, &error, &reply);

            // free the message
            sd_bus_message_unref(msg);

            // and check the reply
            if ((rc < 0) || !reply)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to send method call message (%s - %s)",
                                 error.name, error.message);
                return;
            }

            // get the reply type and check if an error
            uint8_t type;
            sd_bus_message_get_type(reply, &type);
            if (type == SD_BUS_MESSAGE_METHOD_ERROR)
            {
                const sd_bus_error *err = sd_bus_message_get_error(reply);

                AI_LOG_WARN("method call %s.%s failed with error %s - '%s'",
                            method.interface.c_str(), method.name.c_str(),
                            err ? err->name : "", err ? err->message : "");
            }
            else
            {
                // demarshall the arguments from the reply
                replyArgs = SDBusArguments::demarshallArgs(reply);

                // succeeded
                success = true;
            }

            // free the reply
            sd_bus_message_unref(reply);
        };

    // run the lambda to emit the signal
    if (!runOnEventLoopThread(std::move(methodCallLambda)))
    {
        AI_LOG_ERROR("failed to execute function to call method");
        return false;
    }

    // return the result
    return success;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
bool SDBusIpcService::emitSignal(const Signal &signal, const VariantList &args)
{
    // lambda to run in the context of the event loop thread
    bool success = false;
    std::function<void()> emitSignalLambda =
        [&]()
        {
            // construct the signal message
            sd_bus_message *msg = nullptr;
            int rc = sd_bus_message_new_signal(mSDBus, &msg,
                                               signal.object.c_str(),
                                               signal.interface.c_str(),
                                               signal.name.c_str());
            if ((rc < 0) || !msg)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to create new signal message");
                return;
            }

            // marshall the arguments into it
            SDBusArguments::marshallArgs(msg, args);

            // and fire it off
            rc = sd_bus_send(mSDBus, msg, nullptr);
            if (rc < 0)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to send signal message");
            }
            else
            {
                success = true;
            }

            // free the message
            sd_bus_message_unref(msg);
        };

    // run the lambda to emit the signal
    if (!runOnEventLoopThread(std::move(emitSignalLambda)))
    {
        AI_LOG_ERROR("failed to execute function to emit signals");
        return false;
    }

    // return the result
    return success;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
std::string SDBusIpcService::registerMethodHandler(const Method &method,
                                                   const MethodHandler &handler)
{
    // lambda to run in the context of the event loop thread
    std::string tag;
    std::function<void()> registerLambda =
        [&]()
        {
            // check if we already have a slot for the object
            bool existing = false;
            sd_bus_slot *objectSlot = nullptr;
            for (const auto &it : mMethodHandlers)
            {
                const RegisteredMethod &regMethod = it.second;
                if (regMethod.path == method.object)
                {
                    objectSlot = sd_bus_slot_ref(regMethod.objectSlot);

                    // go on and check this method doesn't match the requested
                    existing = (regMethod.interface == method.interface) &&
                               (regMethod.name == method.name);

                    break;
                }
            }

            // if already registered don't register again
            if (existing)
            {
                AI_LOG_WARN("already have registered meothd handler for %s.%s",
                            method.interface.c_str(), method.name.c_str());
                return;
            }

            // if not registered, register the object now
            if (!objectSlot)
            {
                int rc = sd_bus_add_object(mSDBus, &objectSlot, method.object.c_str(),
                                          (sd_bus_message_handler_t)&SDBusIpcService::onMethodCall,
                                          this);
                if (rc < 0)
                {
                    AI_LOG_SYS_ERROR(-rc, "failed to add dbus object listener");
                    return;
                }
            }

            // generate a new unique string for the match
            tag = std::to_string(++mHandlerTag);

            // add to the map
            mMethodHandlers.emplace(tag, RegisteredMethod(objectSlot, method, handler));
        };

    // run the lambda to register the object method call
    if (!runOnEventLoopThread(std::move(registerLambda)))
    {
        AI_LOG_ERROR("failed to execute function to register object");
        return std::string();
    }

    // return the tag (maybe empty if lambda failed)
    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
std::string SDBusIpcService::registerSignalHandler(const Signal &signal,
                                                   const SignalHandler &handler)
{
    // lambda to run in the context of the event loop thread
    std::string tag;
    std::function<void()> registerLambda =
        [&]()
        {
            // generate the match rule
            std::ostringstream matchRule;
            matchRule << "type='signal'";

            if (!signal.object.empty())
                matchRule << ",path='" << signal.object << "'";

            if (!signal.interface.empty())
                matchRule << ",interface='" << signal.interface << "'";

            if (!signal.name.empty())
                matchRule << ",member='" << signal.name << "'";

            // add the match rule
            sd_bus_slot *slot = nullptr;
            int rc = sd_bus_add_match(mSDBus, &slot, matchRule.str().c_str(),
                                     (sd_bus_message_handler_t)&SDBusIpcService::onRuleMatch,
                                     this);
            if ((rc < 0) || !slot)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to add dbus match rule for signal");
                return;
            }

            // generate a new unique string for the match
            tag = std::to_string(++mHandlerTag);

            // add to the map
            mSignalHandlers.emplace(tag, RegisteredSignal(slot, handler));
        };

    // run the lambda to register the object method call
    if (!runOnEventLoopThread(std::move(registerLambda)))
    {
        AI_LOG_ERROR("failed to execute function to register object");
        return std::string();
    }

    // return the tag (maybe empty if lambda failed)
    return tag;
}

// -----------------------------------------------------------------------------
/*!
    \overload


 */
bool SDBusIpcService::unregisterHandler(const std::string& regId)
{
    // lambda to run in the context of the event loop thread
    bool success = false;
    std::function<void()> unregisterLambda =
        [&]()
        {
            // find the handler in the methods
            if (!success)
            {
                auto it = mMethodHandlers.find(regId);
                if (it != mMethodHandlers.end())
                {
                    const RegisteredMethod &method = it->second;
                    sd_bus_slot_unref(method.objectSlot);

                    mMethodHandlers.erase(it);
                    success = true;
                }
            }

            // find the handler in the signals
            if (!success)
            {
                auto it = mSignalHandlers.find(regId);
                if (it != mSignalHandlers.end())
                {
                    const RegisteredSignal &signal = it->second;
                    sd_bus_slot_unref(signal.matchSlot);

                    mSignalHandlers.erase(it);
                    success = true;
                }
            }
        };

    // run the lambda to unregister the object method call
    if (!runOnEventLoopThread(std::move(unregisterLambda)))
    {
        AI_LOG_ERROR("failed to execute function to register object");
        return false;
    }

    // return true if we found the handler and removed it
    return success;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from the an instance of the SDBusAsyncReplySender if the object is
    destroyed without a reply being sent.

 */
void SDBusIpcService::freeMethodReply(uint32_t replyId)
{
    // lambda to run in the context of the event loop thread
    std::function<void()> freeReplyLambda =
        [&]()
        {
            auto it = mCallReplies.find(replyId);
            if (it == mCallReplies.end())
            {
                AI_LOG_ERROR("failed to find reply for reply id %d", replyId);
            }
            else
            {
                // free the reply, return id to pool and remove from the map
                sd_bus_message_unref(it->second);
                mReplyIdentifiers.push(it->first);
                mCallReplies.erase(it);
            }
        };

    // run the lambda to register the object method call
    if (!runOnEventLoopThread(std::move(freeReplyLambda)))
    {
        AI_LOG_ERROR("failed to execute function to free method reply");
    }
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from the an instance of the SDBusAsyncReplySender when the caller
    wants to send a reply back.

 */
bool SDBusIpcService::sendMethodReply(uint32_t replyId,
                                      const AI_IPC::VariantList& replyArgs)
{
    // lambda to run in the context of the event loop thread
    bool success = false;
    std::function<void()> sendReplyLambda =
        [&]()
        {
            // find the reply message
            auto it = mCallReplies.find(replyId);
            if (it == mCallReplies.end())
            {
                AI_LOG_ERROR("failed to find reply for reply id %d", replyId);
                return;
            }

            // get the message, return id to pool and remove from the map
            sd_bus_message *msg = it->second;
            mReplyIdentifiers.push(it->first);
            mCallReplies.erase(it);

            // add some arguments to it
            SDBusArguments::marshallArgs(msg, replyArgs);

            // then send it
            int rc = sd_bus_send(mSDBus, msg, nullptr);
            if (rc < 0)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to send dbus method call reply");
            }
            else
            {
                success = true;
            }

            // free the reply
            sd_bus_message_unref(msg);
        };

    // run the lambda to register the object method call
    if (!runOnEventLoopThread(std::move(sendReplyLambda)))
    {
        AI_LOG_ERROR("failed to execute function to send method reply");
        return false;
    }

    return success;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called from the an instance of the SDBusAsyncReplySender if the client
    requests the uid for a method call.

 */
uid_t SDBusIpcService::getSenderUid(const std::string &senderName)
{
    // lambda to run in the context of the event loop thread
    uid_t userId = -1;
    std::function<void()> getUidLambda =
        [&]()
        {
            // request the creds from the remote device
            sd_bus_creds *creds = nullptr;
            int rc = sd_bus_get_name_creds(mSDBus, senderName.c_str(),
                                           SD_BUS_CREDS_UID, &creds);
            if ((rc < 0) || !creds)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to get uid for sender '%s'",
                                 senderName.c_str());
                return;
            }

            // get the uid
            rc = sd_bus_creds_get_uid(creds, &userId);
            if (rc < 0)
            {
                AI_LOG_SYS_ERROR(-rc, "failed to get uid from creds for sender '%s'",
                                 senderName.c_str());
                userId = -1;
            }

            // free the creds
            sd_bus_creds_unref(creds);
        };

    // run the lambda to register the object method call
    if (!runOnEventLoopThread(std::move(getUidLambda)))
    {
        AI_LOG_ERROR("failed to execute function to get sender uid");
        return -1;
    }

    // return the user id, maybe invalid if we've failed
    return userId;
}

// -----------------------------------------------------------------------------
/*!
    \internal
    \threadsafe

    Runs the given function \a fn on the event loop thread.

 */
bool SDBusIpcService::runOnEventLoopThread(std::function<void()> &&fn) const
{
    // sanity check the event loop thread is running
    if (!mThread.joinable())
    {
        AI_LOG_WARN("sd-bus event loop thread not running");
        return false;
    }

    // if already on the event loop thread then just execute the function
    if (std::this_thread::get_id() == mThread.get_id())
    {
        fn();
        return true;
    }

    // otherwise add to the queue and wait till processed
    std::unique_lock<std::mutex> locker(mExecLock);

    // add to the queue
    const uint64_t tag = ++mExecCounter;
    mExecQueue.emplace_back(tag, std::move(fn));

    // wake the event loop
    uint64_t wake = 1;
    if (TEMP_FAILURE_RETRY(write(mExecEventFd, &wake, sizeof(wake))) != sizeof(wake))
    {
        AI_LOG_SYS_ERROR(errno, "failed to write to eventfd to wake loop");
        mExecQueue.pop_back();
        return false;
    }

    // then wait for the function to be executed
    while (mLastExecTag < tag)
    {
        // wait with a timeout for debugging, we log an error if been waiting
        // for over a second, which would indicate a lock up somewhere
        if (mExecCond.wait_for(locker, std::chrono::seconds(1)) == std::cv_status::timeout)
        {
            AI_LOG_WARN("been waiting for over a second for function to "
                        "execute, soft lock-up occurred?");
        }
    }

    return true;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Static function called when one of our installed match rules captures a
    dbus signal.

 */
int SDBusIpcService::onRuleMatch(sd_bus_message *msg, void *userData, void *retError)
{
    (void)retError;

    SDBusIpcService *self = reinterpret_cast<SDBusIpcService*>(userData);

    // if not started don't do anything
    if (!self->mStarted)
    {
        return 0;
    }

    // get the current slot, used to find the match rule that hit
    sd_bus_slot *slot = sd_bus_get_current_slot(self->mSDBus);
    if (!slot)
    {
        AI_LOG_WARN("match callback called without valid slot");
        return -1;
    }

    // demarshall the args
    const VariantList args = SDBusArguments::demarshallArgs(msg);

    // lookup the match rule using the slot
    for (const auto &handlers : self->mSignalHandlers)
    {
        const RegisteredSignal &signal = handlers.second;
        if (signal.matchSlot == slot)
        {
            if (signal.callback)
                signal.callback(args);
        }
    }

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Static function called when a method call has been sent to an object we've
    installed a method handler for.

 */
int SDBusIpcService::onMethodCall(sd_bus_message *call, void *userData, void *retError)
{
    (void)retError;

    SDBusIpcService *self = reinterpret_cast<SDBusIpcService*>(userData);

    // get the object path, interface and method for the lookup
    const char *path = sd_bus_message_get_path(call);
    const char *interface = sd_bus_message_get_interface(call);
    const char *member = sd_bus_message_get_member(call);
    if (!path || !interface || !member)
    {
        AI_LOG_ERROR("failed to get required fields from method call");
        return 0;
    }

    // get a reply identifier
    if (self->mReplyIdentifiers.empty())
    {
        AI_LOG_ERROR("reply identifier pool is empty");
        return 0;
    }
    uint32_t replyId = self->mReplyIdentifiers.front();
    self->mReplyIdentifiers.pop();

    AI_LOG_DEBUG("processing method call %s.%s", interface, member);

    // find the handler, unless not yet started
    bool handled = false;
    int rc;
    if (self->mStarted)
    {
        for (const auto &handlers : self->mMethodHandlers)
        {
            const RegisteredMethod &method = handlers.second;

            if ((method.name == member) &&
                (method.interface == interface) &&
                (method.path == path) &&
                method.callback)
            {
                // get the sender uid, this will probably fail, but not an issue
                // as the SDBusAsyncReplySender object will re-issue the uid
                // request if needed
                uid_t senderUid;
                sd_bus_creds *creds = sd_bus_message_get_creds(call);
                if (!creds || (sd_bus_creds_get_uid(creds, &senderUid) != 0))
                {
                    senderUid = -1;
                }

                // create the reply message and store it against the reply id
                sd_bus_message *reply = nullptr;
                rc = sd_bus_message_new_method_return(call, &reply);
                if (rc < 0)
                {
                    AI_LOG_SYS_ERROR(-rc, "failed to create method call reply");
                    return rc;
                }

                // store the reply against the id
                self->mCallReplies.emplace(replyId, reply);

                // create the reply sender object
                std::shared_ptr<SDBusAsyncReplySender> sender =
                    std::make_shared<SDBusAsyncReplySender>(self->shared_from_this(),
                                                            replyId,
                                                            sd_bus_message_get_sender(call),
                                                            senderUid,
                                                            SDBusArguments::demarshallArgs(call));

                // call the handler
                method.callback(sender);

                // indicate we had a handler
                handled = true;
                break;
            }
        }
    }

    AI_LOG_DEBUG("finished method call %s.%s (handled: %s)",
                 interface, member, handled ? "yes" : "no");

    // if no handler was invoked then send an error reply
    if (!handled)
    {
        rc = sd_bus_reply_method_errorf(call,
                                        "org.freedesktop.DBus.Error.UnknownMethod",
                                        "No handler for method %s.%s",
                                        interface, member);
        return rc;
    }

    return 1;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Static function called when a reply is received for a method call we've made.

 */
int SDBusIpcService::onMethodReply(sd_bus_message *reply, void *userData, void *retError)
{
    (void)retError;

    SDBusIpcService *self = reinterpret_cast<SDBusIpcService*>(userData);

    // get the cookie to find the correct callback
    uint64_t cookie;
    int rc = sd_bus_message_get_reply_cookie(reply, &cookie);
    if (rc < 0)
    {
        AI_LOG_SYS_ERROR(-rc, "failed to get cookie of reply message");
        return 0;
    }

    // nb: we don't need to take any locking for this as we know we'll only
    // be called from the dbus event loop thread
    auto it = self->mCalls.find(cookie);
    if (it == self->mCalls.end())
    {
        AI_LOG_ERROR("failed to find callback for cookie %" PRIu64, cookie);
        return 0;
    }

    // get a copy of the callback and erase from the map
    std::shared_ptr<SDBusAsyncReplyGetter> replyGetter = it->second;
    self->mCalls.erase(it);

    // add the reply to the getter
    if (replyGetter)
    {
        // get the reply type
        uint8_t type = _SD_BUS_MESSAGE_TYPE_INVALID;
        rc = sd_bus_message_get_type(reply, &type);
        if (rc < 0)
        {
            AI_LOG_SYS_ERROR(-rc, "failed to get message type");
        }

        // if an error call the error callback
        if (type == SD_BUS_MESSAGE_METHOD_RETURN)
        {
            // complete the reply with args
            replyGetter->setReply(true, SDBusArguments::demarshallArgs(reply));
        }
        else
        {
            // otherwise assume an error
            const sd_bus_error *error = sd_bus_message_get_error(reply);
            if (error)
            {
                AI_LOG_WARN("error reply to method call %s - %s",
                            error->name, error->message);
            }
            else
            {
                AI_LOG_WARN("method call failed with unknown error");
            }

            // either way the method call is finished
            replyGetter->setReply(false, { });
        }
    }

    // done
    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Static function called when the mExecEventFd is written to.  Here we
    process all the 'executors' that have been queued.

 */
int SDBusIpcService::onExecCall(sd_event_source *s, int fd, uint32_t revents,
                                void *userData)
{
    (void)s;
    (void)revents;

    SDBusIpcService *self = reinterpret_cast<SDBusIpcService*>(userData);
    AI_DEBUG_ASSERT(fd == self->mExecEventFd);

    // read the eventfd to clear it
    uint64_t value;
    if (TEMP_FAILURE_RETRY(read(fd, &value, sizeof(value))) != sizeof(value))
    {
        AI_LOG_SYS_ERROR(errno, "failed to read from eventfd");
    }

    // take the lock and process all the queued executors
    {
        std::lock_guard<std::mutex> locker(self->mExecLock);

        while (!self->mExecQueue.empty())
        {
            Executor exec = std::move(self->mExecQueue.front());
            self->mExecQueue.pop_front();

            if (exec.func)
                exec.func();

            self->mLastExecTag = exec.tag;
        }
    }

    // unblock the thread(s) that made the call(s)
    self->mExecCond.notify_all();

    return 0;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    The worker thread function that runs the sd-bus event loop.

 */
void SDBusIpcService::eventLoopThread()
{
    AI_LOG_INFO("started sd-bus event loop thread");

    // set a friendly name for the thread
    pthread_setname_np(pthread_self(), "AI_IPC_SDBUS");

    // create the event loop
    sd_event *loop = nullptr;
    int rc = sd_event_default(&loop);
    if ((rc < 0) || !loop)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to create new event loop");
        return;
    }

    // install a handler for the exec eventfd
    rc = sd_event_add_io(loop, nullptr, mExecEventFd, EPOLLIN,
                         &SDBusIpcService::onExecCall, this);
    if (rc < 0)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to install handler for exec events");
        return;
    }

    // add the sdbus to the event loop
    rc = sd_bus_attach_event(mSDBus, loop, SD_EVENT_PRIORITY_NORMAL);
    if (rc < 0)
    {
        AI_LOG_SYS_FATAL(-rc, "failed to add dbus to event loop");
        return;
    }

    AI_LOG_INFO("starting sd-bus event loop");

    // run the event loop (blocking call)
    sd_event_loop(loop);

    AI_LOG_INFO("stopping sd-bus event loop");

    // detach from the event loop and flush everything out of the bus
    sd_bus_detach_event(mSDBus);
    sd_bus_flush(mSDBus);

    // close and free the dbus
    sd_bus_flush_close_unref(mSDBus);
    mSDBus = nullptr;

    // free the event loop
    sd_event_unref(loop);

    // done
}

