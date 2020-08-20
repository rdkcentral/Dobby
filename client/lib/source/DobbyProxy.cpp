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
 * DobbyProxy.cpp
 * Author:
 *
 */
#include "DobbyProxy.h"

#include <DobbyProtocol.h>

#include <Logging.h>

#include <thread>


// -----------------------------------------------------------------------------
/**
 *  @class Minimal dispatcher used for all Dobby notification events.
 *
 *  Since we already run a different thread to send out notificiations there is
 *  no need for this to do anything fancy, just call the notification function.
 *
 */
class DobbyProxyNotifyDispatcher : public AICommon::IDispatcher
{
public:
    DobbyProxyNotifyDispatcher() = default;
    ~DobbyProxyNotifyDispatcher() final = default;

public:
    void post(std::function<void ()> work) override
    {
        work();
    }

    void sync() override
    {
    }

    bool invokedFromDispatcherThread() override
    {
        return false;
    }
};


// -----------------------------------------------------------------------------
/**
 *  @brief Registers the signal handlers and sets up a threaded dispatcher
 *  for started / stopped events.
 *
 *
 *
 */
DobbyProxy::DobbyProxy(const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
                       const std::string& serviceName,
                       const std::string& objectName)
    : mIpcService(ipcService)
    , mServiceName(serviceName)
    , mObjectName(objectName)
{
    AI_LOG_FN_ENTRY();

    // sets the basic dispatcher
    setDispatcher(std::make_shared<DobbyProxyNotifyDispatcher>());

    // start the thread for emitting container state change events
    mStateChangeThread = std::thread(&DobbyProxy::containerStateChangeThread, this);

    // install signal handlers for the container start / stop events
    const AI_IPC::Signal startedSignal(objectName, DOBBY_CTRL_INTERFACE, DOBBY_CTRL_EVENT_STARTED);
    const AI_IPC::SignalHandler startedHandler(std::bind(&DobbyProxy::onContainerStartedEvent, this, std::placeholders::_1));
    mContainerStartedSignal = mIpcService->registerSignalHandler(startedSignal, startedHandler);

    const AI_IPC::Signal stoppedSignal(objectName, DOBBY_CTRL_INTERFACE, DOBBY_CTRL_EVENT_STOPPED);
    const AI_IPC::SignalHandler stoppedHandler(std::bind(&DobbyProxy::onContainerStoppedEvent, this, std::placeholders::_1));
    mContainerStoppedSignal = mIpcService->registerSignalHandler(stoppedSignal, stoppedHandler);

    if (mContainerStartedSignal.empty() || mContainerStoppedSignal.empty())
    {
        AI_LOG_ERROR("failed to register dbus signal listeners");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Unregisters the signal listeners and flushes the ipc connection.
 *
 *
 *
 */
DobbyProxy::~DobbyProxy()
{
    AI_LOG_FN_ENTRY();

    // unregister the signal handlers
    if (!mContainerStartedSignal.empty())
        mIpcService->unregisterHandler(mContainerStartedSignal);

    if (!mContainerStoppedSignal.empty())
        mIpcService->unregisterHandler(mContainerStoppedSignal);

    // flush the ipc service to guarantee the signal handlers aren't going to
    // be called after we're done
    mIpcService->flush();

    // can now safely stop the dispatcher thread
    if (mStateChangeThread.joinable())
    {
        std::unique_lock<std::mutex> locker(mStateChangeLock);
        mStateChangeQueue.emplace_back(StateChangeEvent::Terminate);
        locker.unlock();

        mStateChangeCond.notify_all();
        mStateChangeThread.join();
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Installs a callback 'listener' to be notified of changes to the
 *  state of the containers.
 *
 *  On success a positive id value will be returned for the listener, this
 *  should then be passed to unregisterListener(...) to release the listener.
 *
 *  @param[in]  listener    The callback to install.
 *  @param[in]  cbParams    Pointer to custom parameters.
 */
int DobbyProxy::registerListener(const StateChangeListener &listener, const void* cbParams)
{
    std::lock_guard<std::mutex> locker(mListenersLock);

    std::pair<StateChangeListener, const void*> cbData = std::make_pair(listener, cbParams);

    int id = mListenerIdGen.get();
    if (id < 0)
    {
        AI_LOG_ERROR("too many listeners installed");
        return -1;
    }

    mListeners.emplace(id, cbData);
    return id;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Unregisters a listener previously registered.
 *
 *  Do no call this from the context of a listener callback.
 *
 *  @param[in]  id      The id of the listener returned by registerListener.
 */
void DobbyProxy::unregisterListener(int id)
{
    std::lock_guard<std::mutex> locker(mListenersLock);

    std::map<int, std::pair<StateChangeListener, const void*>>::iterator it = mListeners.find(id);
    if (it == mListeners.end())
    {
        AI_LOG_ERROR("no listener installed with id %d", id);
        return;
    }

    mListeners.erase(it);
    mListenerIdGen.put(id);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called when a com.sky.dobby.ctrl1.Started event is received from
 *  the Dobby 'hypervisor' daemon
 *
 *  We parse the event data and if it makes sense we bounce this event up to any
 *  listeners using the Notifier / Observer pattern.
 *
 *  @param[in]  args        The args sent with the event.
 */
void DobbyProxy::onContainerStartedEvent(const AI_IPC::VariantList& args)
{
    AI_LOG_FN_ENTRY();

    // the event should container two args; container descriptor and id
    int32_t descriptor;
    std::string id;

    if (!AI_IPC::parseVariantList<int32_t, std::string>(args, &descriptor, &id))
    {
        AI_LOG_ERROR("failed to read all args from %s.%s signal",
                     DOBBY_CTRL_INTERFACE, DOBBY_CTRL_EVENT_STARTED);
    }
    else
    {
        // ping off an event
        std::lock_guard<std::mutex> locker(mStateChangeLock);
        mStateChangeQueue.emplace_back(StateChangeEvent::ContainerStarted, descriptor, id);
        mStateChangeCond.notify_all();
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called when a com.sky.dobby.ctrl1.Stopped event is received from
 *  the Dobby 'hypervisor' daemon
 *
 *  We parse the event data and if it makes sense we bounce this event up to any
 *  listeners using the Notifier / Observer pattern.
 *
 *  @param[in]  args        The args sent with the event.
 */
void DobbyProxy::onContainerStoppedEvent(const AI_IPC::VariantList& args)
{
    AI_LOG_FN_ENTRY();

    // the event should container two args; container descriptor and id
    int32_t descriptor;
    std::string id;

    if (!AI_IPC::parseVariantList<int32_t, std::string>(args, &descriptor, &id))
    {
        AI_LOG_ERROR("failed to read all args from %s.%s signal",
                     DOBBY_CTRL_INTERFACE, DOBBY_CTRL_EVENT_STOPPED);
    }
    else
    {
        // ping off an event
        std::lock_guard<std::mutex> locker(mStateChangeLock);
        mStateChangeQueue.emplace_back(StateChangeEvent::ContainerStopped, descriptor, id);
        mStateChangeCond.notify_all();
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Invokes a dbus method on the daemon.
 *
 *  The method is invoked with the service name and object name that was set
 *  in the constructor.
 *
 *
 *  @param[in]  interface_      The dbus interface name of the method
 *  @param[in]  method_         The dbus method name to invoke
 *  @param[in]  params_         The list of args to apply
 *  @param[in]  returns_        Reference variable that the results will be put
 *                              in on success.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::invokeMethod(const char *interface_,
                              const char *method_,
                              const AI_IPC::VariantList& params_,
                              AI_IPC::VariantList& returns_) const
{
    const AI_IPC::Method method(mServiceName, mObjectName, interface_, method_);
    if (!mIpcService->invokeMethod(method, params_, returns_))
    {
        AI_LOG_ERROR("failed to invoke '%s.%s'", method.interface.c_str(),
                     method.name.c_str());
        return false;
    }
    else
    {
        return true;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Checks if the daemon is alive
 *
 *  This function just polls on the service becoming available on the bus, the
 *  poll period is 20ms and it will keep polling until either the service
 *  is present or the timeout is exceeded.
 *
 *  @param[in]  timeout     The number of milliseconds to wait for the service.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::isAlive(const std::chrono::milliseconds& timeout /*= std::chrono::milliseconds::min()*/) const
{
    AI_LOG_FN_ENTRY();

    // the time to sleep for in each iteration of the poll loop
    std::chrono::milliseconds pollSleep(20);

    // calculate the timeout time
    std::chrono::time_point<std::chrono::steady_clock> timeoutAfter;
    if (timeout == std::chrono::milliseconds::min())
    {
        timeoutAfter = std::chrono::steady_clock::now() + std::chrono::milliseconds::max();
    }
    else
    {
        timeoutAfter = std::chrono::steady_clock::now() + timeout - pollSleep;
    }

    // poll on the service being available within the given timeout
    while (!mIpcService->isServiceAvailable(mServiceName))
    {
        if (std::chrono::steady_clock::now() > timeoutAfter)
        {
            AI_LOG_ERROR_EXIT("timed-out waiting for the '%s' service to arrive"
                              " on the bus", mServiceName.c_str());
            return false;
        }

        std::this_thread::sleep_for(pollSleep);
    }

    return true;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Asks the daemon to shut itself down
 *
 *  The daemon is expected to reply with a value before shutting down.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::shutdown() const
{
    AI_LOG_FN_ENTRY();

    const AI_IPC::VariantList params = { };
    AI_IPC::VariantList returns;

    AI_LOG_FN_EXIT();
    return invokeMethod(DOBBY_ADMIN_INTERFACE,
                        DOBBY_ADMIN_METHOD_SHUTDOWN,
                        params, returns);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Asks the daemon to send back a pong message
 *
 *
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::ping() const
{
    AI_LOG_FN_ENTRY();

    AI_IPC::VariantList returns;

    AI_LOG_FN_EXIT();
    return invokeMethod(DOBBY_ADMIN_INTERFACE,
                        DOBBY_ADMIN_METHOD_PING,
                        { }, returns);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the AI dbus address for use by the containeriser
 *
 *  The dobby daemon itself doesn't use the AI dbuses, rather it stores them
 *  and provides the addresses to any plugins and any container that requested
 *  them in it's spec file.
 *
 *  @param[in]  privateBus      true if the address is for the AI private
 *                              bus, otherwise the AI public bus.
 *  @param[in]  address         The dbus address to set.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::setAIDbusAddress(bool privateBus,
                                  const std::string& address) const
{
    AI_LOG_FN_ENTRY();

    const AI_IPC::VariantList params = { privateBus,
                                         address };
    AI_IPC::VariantList returns;

    bool result = false;

    if (invokeMethod(DOBBY_ADMIN_INTERFACE,
                     DOBBY_ADMIN_METHOD_SET_AI_DBUS_ADDR,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Sets the logging method used by the daemon.
 *
 *  By default the dobby daemon logs to syslog, by sending a valid pipe fd
 *  to the daemon it will switch to routing all it's logs via the supplied
 *  pipe.
 *
 *  This is typically called by DobbyFactory right after the daemon is launched.
 *
 *  The log @a method should be one of the following values:
 *
 *      DOBBY_LOG_NULL     : disable all diag based log output
 *      DOBBY_LOG_SYSLOG   : route diag logging to syslog
 *      DOBBY_LOG_ETHANLOG : route diag logging to the supplied pipe
 *
 *  @param[in]  method      The log method to use.
 *  @param[in]  pipeFd      The write fd of the pipe.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::setLogMethod(uint32_t method, int pipeFd /*= -1*/) const
{
    AI_LOG_FN_ENTRY();

    AI_IPC::VariantList params;

    if ((method == DOBBY_LOG_NULL) || (method == DOBBY_LOG_SYSLOG))
    {
        params.emplace_back(method);
    }
    else if (method == DOBBY_LOG_ETHANLOG)
    {
        if (pipeFd < 0)
        {
            AI_LOG_ERROR_EXIT("must supply a pipeFd if setting log method to 'ethan'");
            return false;
        }

        params.emplace_back(method);
        params.emplace_back(AI_IPC::UnixFd(pipeFd));
    }
    else
    {
        AI_LOG_ERROR_EXIT("invalid logging method (%u)", method);
        return false;
    }

    bool result = false;
    AI_IPC::VariantList returns;

    if (invokeMethod(DOBBY_ADMIN_INTERFACE,
                     DOBBY_ADMIN_METHOD_SET_LOG_METHOD,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Simply sets the log level in the daemon
 *
 *  The value should be one of the constants from the Logging.h header, i.e.
 *
 *      AI_DEBUG_LEVEL_FATAL
 *      AI_DEBUG_LEVEL_ERROR
 *      AI_DEBUG_LEVEL_WARNING
 *      AI_DEBUG_LEVEL_MILESTONE
 *      AI_DEBUG_LEVEL_INFO
 *      AI_DEBUG_LEVEL_DEBUG
 *
 *  @param[in]  level       The log level to set
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::setLogLevel(int level) const
{
    AI_LOG_FN_ENTRY();

    const AI_IPC::VariantList params = { int32_t(level) };
    AI_IPC::VariantList returns;
    bool result = false;

    if (invokeMethod(DOBBY_ADMIN_INTERFACE,
                     DOBBY_ADMIN_METHOD_SET_LOG_METHOD,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts a container with the given id, json spec file and the list
 *  of files.
 *
 *
 *
 *  @param[in]  id              The string id of the container, this should not
 *                              have any spaces and only container alphanumeric
 *                              characters plus '.' and '-'.
 *  @param[in]  jsonSpec        The (large) string containing the json formatted
 *                              container spec.
 *  @param[in]  files           An array of file descriptors to pass into the
 *                              container construction, can be empty.
 *  @param[in]  command         Custom command to run inside the container,
 *                              overriding the args in the config file
 *  @param[in]  displaySocket   Path to the westeros display socket to mount into
 *                              the container
 *
 *  @return on success a container descriptor which is a number greater than
 *  0, on failure a negative error code.
 */
int32_t DobbyProxy::startContainerFromSpec(const std::string& id,
                                           const std::string& jsonSpec,
                                           const std::list<int>& files /*= std::list<int>()*/,
                                           const std::string& command /*= ""*/,
                                           const std::string& displaySocket /*= ""*/) const
{
    AI_LOG_FN_ENTRY();

    // convert file descriptors into unixfd objects
    std::vector<AI_IPC::UnixFd> fds;
    for (int fd : files)
    {
        fds.emplace_back(AI_IPC::UnixFd(fd));
    }

    // send off the request
    const AI_IPC::VariantList params = { id, jsonSpec, fds, command, displaySocket };
    AI_IPC::VariantList returns;

    int32_t result = -1;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_START_FROM_SPEC,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<int32_t>(returns, &result))
        {
            result = -1;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts a container with the given id, bundle path and the list
 *  of files.
 *
 *
 *  @param[in]  id              The string id of the container, this should not
 *                              have any spaces and only container alphanumeric
 *                              characters plus '.' and '-'.
 *  @param[in]  bundlePath      Path to the container bundle.
 *  @param[in]  files           An array of file descriptors to pass into the
 *                              container construction, can be empty.
 *  @param[in]  command         Custom command to run inside the container,
 *                              overriding the args in the config file
 *  @param[in]  displaySocket   Path to the westeros display socket to mount into
 *                              the container
 *
 *  @return on success a container descriptor which is a number greater than
 *  0, on failure a negative error code.
 */
int32_t DobbyProxy::startContainerFromBundle(const std::string& id,
                                             const std::string& bundlePath,
                                             const std::list<int>& files,
                                             const std::string& command /*= ""*/,
                                             const std::string& displaySocket /*= ""*/) const
{
    AI_LOG_FN_ENTRY();

    // convert file descriptors into unixfd objects
    std::vector<AI_IPC::UnixFd> fds;
    for (int fd : files)
    {
        fds.emplace_back(AI_IPC::UnixFd(fd));
    }

    // send off the request
    const AI_IPC::VariantList params = { id, bundlePath, fds, command, displaySocket };
    AI_IPC::VariantList returns;

    int32_t result = -1;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_START_FROM_BUNDLE,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<int32_t>(returns, &result))
        {
            result = -1;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stops the container with the descriptor (container integer id)
 *
 *  @warning A positive response doesn't mean the container has stopped, rather
 *  it means the container has successfully been asked to stop. To determine
 *  when a container has stopped you need to observe the container status
 *  events.
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *  @param[in]  withPrejudice   If true the container is terminated using a
 *                              SIGKILL, the default is false in which case
 *                              the container is asked to close with a SIGTERM.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::stopContainer(int32_t cd, bool withPrejudice /*= false*/) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd, withPrejudice };
    AI_IPC::VariantList returns;

    bool result = false;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_STOP,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Pauses the container with the descriptor (container integer id)
 *
 *  @warning A positive response doesn't mean the container has paused, rather
 *  it means the container has successfully been asked to pause. To determine
 *  when a container has paused you need to observe the container status
 *  events.
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::pauseContainer(int32_t cd) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd };
    AI_IPC::VariantList returns;

    bool result = false;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_PAUSE,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Resumes the container with the descriptor (container integer id)
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::resumeContainer(int32_t cd) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd };
    AI_IPC::VariantList returns;

    bool result = false;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_RESUME,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Executes a command in the given container.
 *
 *  @param[in]  cd              The container descriptor.
 *  @param[in]  options         The options to execute the command with.
 *  @param[in]  command         The command to execute.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::execInContainer(int32_t cd, const std::string& options, const std::string& command) const
{
    AI_LOG_FN_ENTRY();

    // Send off the request
    const AI_IPC::VariantList params = { cd, options, command };
    AI_IPC::VariantList returns;

    bool result = false;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_EXEC,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns the current state of a container.
 *
 *  The container state will be one of the following values
 *      CONTAINER_STATE_INVALID
 *      CONTAINER_STATE_STARTING
 *      CONTAINER_STATE_RUNNING
 *      CONTAINER_STATE_STOPPING
 *      CONTAINER_STATE_PAUSED
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *
 *  @return on success a positive state value, on failure -1.
 */
int DobbyProxy::getContainerState(int32_t cd) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd };
    AI_IPC::VariantList returns;

    int32_t result = 0;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_GETSTATE,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<int32_t>(returns, &result))
        {
            result = -1;
        }
    }

    IDobbyProxyEvents::ContainerState state;
    switch (result)
    {
        case CONTAINER_STATE_STARTING:
            state = IDobbyProxyEvents::ContainerState::Starting;
            break;
        case CONTAINER_STATE_RUNNING:
            state = IDobbyProxyEvents::ContainerState::Running;
            break;
        case CONTAINER_STATE_STOPPING:
            state = IDobbyProxyEvents::ContainerState::Stopping;
            break;
        case CONTAINER_STATE_PAUSED:
            state = IDobbyProxyEvents::ContainerState::Paused;
            break;
        case CONTAINER_STATE_INVALID:
        default:
            state = IDobbyProxyEvents::ContainerState::Invalid;
            break;
    }

    AI_LOG_FN_EXIT();
    return static_cast<int>(state);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the stats / info for the given container.
 *
 *  This method returns a string containing info / stats on the given container
 *  in json format.  The json should look something like the following
 *
 *      {
 *          "id": "blah",
 *          "pids": [ 2046, 2064 ],
 *          "cpu": {
 *              "usage": {
 *                  "total":734236982,
 *                  "percpu":[348134887,386102095]
 *              }
 *          },
 *          "memory":{
 *              "user": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          "gpu":{
 *              "memory": {
 *                  "limit":41943040,
 *                  "usage":356352,
 *                  "max":524288,
 *                  "failcnt":0
 *              }
 *          }
 *          ...
 *      }
 *
 *  But be warned it may change over time.
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *
 *  @return the json string on success, on failure an empty string.
 */
std::string DobbyProxy::getContainerInfo(int32_t cd) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd };
    AI_IPC::VariantList returns;

    std::string jsonInfo;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_GETINFO,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<std::string>(returns, &jsonInfo))
        {
            jsonInfo.clear();
        }
    }

    AI_LOG_FN_EXIT();
    return jsonInfo;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns a list of containers
 *
 *  Each element in the returned list is a pair of container descriptor
 *  (integer) and the string id of the container.
 *
 *  This returns a list of containers the daemon knows about, this is not
 *  necessarily only running containers, it will include containers that are
 *  in the starting and stopping states. To determine the state of a container
 *  use the @a DobbyProxy::getContainerState method.
 *
 *  @return a list of all known containers.
 */
std::list<std::pair<int32_t, std::string>> DobbyProxy::listContainers() const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params;
    AI_IPC::VariantList returns;

    std::vector<int32_t> descriptors;
    std::vector<std::string> ids;
    std::list<std::pair<int32_t, std::string>> result;

    if (invokeMethod(DOBBY_CTRL_INTERFACE,
                     DOBBY_CTRL_METHOD_LIST,
                     params, returns))
    {
        if (AI_IPC::parseVariantList
                <std::vector<int32_t>, std::vector<std::string>>
                (returns, &descriptors, &ids))
        {
            // sanity check the arrays are the same size
            if (descriptors.size() != ids.size())
            {
                AI_LOG_ERROR("array size mismatch (%zu vs %zu)",
                             descriptors.size(), ids.size());
            }
            else
            {
                // re-combine the vectors into a single list
                for (size_t i = 0; i < descriptors.size(); i++)
                {
                    result.push_back(std::make_pair(descriptors[i], ids[i]));
                }
            }
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}

#if (AI_BUILD_TYPE == AI_DEBUG)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility that can be used to create a bundle based on
 *  a sky spec file
 *
 *  This can be useful for debugging container issues, as it allows the daemon
 *  to create the bundle but not actually run it, and therefore it can be
 *  run manually from the command line.
 *
 *  @param[in]  id          The name of the bundle to create.
 *  @param[in]  jsonSpec    The json spec for the bundle.
 *
 *  @return true on success, false on failure.
 */
bool DobbyProxy::createBundle(const std::string& id,
                              const std::string& jsonSpec) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { id, jsonSpec };
    AI_IPC::VariantList returns;

    bool result = false;

    if (invokeMethod(DOBBY_DEBUG_INTERFACE,
                     DOBBY_DEBUG_METHOD_CREATE_BUNDLE,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<bool>(returns, &result))
        {
            result = false;
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}
#endif // (AI_BUILD_TYPE == AI_DEBUG)

#if (AI_BUILD_TYPE == AI_DEBUG)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility to retrieve the original spec file for a running
 *  container (i.e. like the 'virsh dumpxml' command).
 *
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *
 *  @return a string containing the spec json, on failure an empty string.
 */
std::string DobbyProxy::getSpec(int32_t cd) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd };
    AI_IPC::VariantList returns;

    std::string result;

    if (invokeMethod(DOBBY_DEBUG_INTERFACE,
                     DOBBY_DEBUG_METHOD_GET_SPEC,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<std::string>(returns, &result))
        {
            result.clear();
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}
#endif // (AI_BUILD_TYPE == AI_DEBUG)

#if (AI_BUILD_TYPE == AI_DEBUG)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility to retrieve the config.json file for a running
 *  container (i.e. like the 'virsh dumpxml' command).
 *
 *
 *  @param[in]  cd              The container descriptor, which is the value
 *                              returned by startContainer call.
 *
 *  @return a string containing the config.json, on failure an empty string.
 */
std::string DobbyProxy::getOCIConfig(int32_t cd) const
{
    AI_LOG_FN_ENTRY();

    // send off the request
    const AI_IPC::VariantList params = { cd };
    AI_IPC::VariantList returns;

    std::string result;

    if (invokeMethod(DOBBY_DEBUG_INTERFACE,
                     DOBBY_DEBUG_METHOD_GET_OCI_CONFIG,
                     params, returns))
    {
        if (!AI_IPC::parseVariantList<std::string>(returns, &result))
        {
            result.clear();
        }
    }

    AI_LOG_FN_EXIT();
    return result;
}
#endif // (AI_BUILD_TYPE == AI_DEBUG)

// -----------------------------------------------------------------------------
/**
 *  @brief Thread function that receives notifications on container state
 *  changes and then calls the install handler(s).
 *
 *  We use a separate thread to notify of container changes because we don't want
 *  to block the IpcService thread for long periods of time while code does
 *  stuff based on container state change.
 *
 *
 */
void DobbyProxy::containerStateChangeThread()
{
    AI_LOG_FN_ENTRY();

    pthread_setname_np(pthread_self(), "AI_DOBBY_PROXY");

    AI_LOG_INFO("entered container state change thread");

    std::unique_lock<std::mutex> locker(mStateChangeLock);

    bool terminate = false;
    while (!terminate)
    {
        // wait for an event
        while (mStateChangeQueue.empty())
        {
            mStateChangeCond.wait(locker);
        }

        // process all events
        while (!mStateChangeQueue.empty())
        {
            // take the first item off the queue
            const StateChangeEvent event = mStateChangeQueue.front();
            mStateChangeQueue.pop_front();

            // drop the lock before calling any callbacks
            locker.unlock();

            if (event.type == StateChangeEvent::Terminate)
            {
                // terminate event so just set the flag so we terminate the
                // thread when the queue is empty
                terminate = true;
            }
            else
            {
                const IDobbyProxyEvents::ContainerState state =
                    (event.type == StateChangeEvent::ContainerStarted) ?
                        IDobbyProxyEvents::ContainerState::Running :
                        IDobbyProxyEvents::ContainerState::Stopped;

                // fire off via the notifier system first (deprecated but
                // required for backwards compatibility)
                notify(&IDobbyProxyEvents::containerStateChanged,
                       event.descriptor, event.name, state);

                // need to hold the lock before searching
                std::lock_guard<std::mutex> listenerLocker(mListenersLock);

                // check if we have any listener interested in this service
                for (const std::pair<const int, std::pair<StateChangeListener, const void*>>& handler : mListeners)
                {
                    const StateChangeListener& callback = handler.second.first;
                    const void* cbParams = handler.second.second;
                    if (callback)
                        callback(event.descriptor, event.name, state, cbParams);
                }
            }

            // re-take the lock and check for any more events
            locker.lock();
        }
    }

    AI_LOG_INFO("exiting container state change thread");

    AI_LOG_FN_EXIT();
}
