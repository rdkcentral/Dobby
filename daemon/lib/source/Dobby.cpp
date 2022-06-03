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
 * File:   Dobby.cpp
 *
 */
#include "Dobby.h"
#include "IDobbySettings.h"
#include "DobbyProtocol.h"
#include "DobbyLogger.h"
#include "DobbyManager.h"
#include "DobbyEnv.h"
#include "DobbyUtils.h"
#include "DobbyIPCUtils.h"
#include "DobbyWorkQueue.h"

#if defined(LEGACY_COMPONENTS)
#  include "DobbyTemplate.h"
#endif // defined(LEGACY_COMPONENTS)

#include <Logging.h>
#include <Tracing.h>

#if defined(AI_ENABLE_TRACING)
    #include <PerfettoTracing.h>
#endif

#if defined(RDK)
    #if defined(USE_SYSTEMD)
        #define SD_JOURNAL_SUPPRESS_LOCATION
        #include <systemd/sd-journal.h>
        #include <systemd/sd-daemon.h>
    #endif
#else
#  include <ethanlog.h>
#endif

#ifdef USE_BREAKPAD
    #include "breakpad_wrapper.h"
#endif

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/syscall.h>
#include <inttypes.h>

volatile sig_atomic_t Dobby::mSigTerm = 0;


/// The target for logging, can be dynamically changed via dbus
std::atomic<unsigned> Dobby::mLogTargets(LogTarget::Console);

/// The fd of the ethan logging pipe, starts off pointing to /dev/null but can
/// changed dynamically via dbus
int Dobby::mEthanLogPipeFd = -1;



Dobby::Dobby(const std::string& dbusAddress,
             const std::shared_ptr<AI_IPC::IIpcService>& ipcService,
             const std::shared_ptr<const IDobbySettings>& settings)
    : mEnvironment(std::make_shared<DobbyEnv>(settings))
    , mUtilities(std::make_shared<DobbyUtils>())
    , mIPCUtilities(std::make_shared<DobbyIPCUtils>(dbusAddress, ipcService))
    , mWorkQueue(new DobbyWorkQueue)
    , mIpcService(ipcService)
    , mService(DOBBY_SERVICE)
    , mObjectPath(DOBBY_OBJECT)
    , mShutdown(false)
    , mWatchdogTimerId(-1)
{
    AI_LOG_FN_ENTRY();

#if defined(LEGACY_COMPONENTS)
    // initialise the template code with the settings
    DobbyTemplate::setSettings(settings);
#endif //defined(LEGACY_COMPONENTS)

    // create the two callback function objects for notifying when a container
    // has start and stopped
    DobbyManager::ContainerStartedFunc startedCb =
        std::bind(&Dobby::onContainerStarted, this,
                  std::placeholders::_1, std::placeholders::_2);

    DobbyManager::ContainerStoppedFunc stoppedCb =
        std::bind(&Dobby::onContainerStopped, this,
                  std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);


    // create the container manager which does all the heavy lifting
    mManager = std::make_shared<DobbyManager>(mEnvironment, mUtilities,
                            mIPCUtilities, settings, startedCb, stoppedCb);
    if (!mManager)
    {
        AI_LOG_FATAL("failed to create manager");
    }

    // setup our dbus ipc interface
    initIpcMethods();

    // enable the notification for the watchdog
#if defined(RDK) && defined(USE_SYSTEMD)
    initWatchdog();
#endif

    AI_LOG_FN_EXIT();
}

Dobby::~Dobby()
{
    AI_LOG_FN_ENTRY();

    // cancel the watchdog timer
    if (mWatchdogTimerId >= 0)
    {
        mUtilities->cancelTimer(mWatchdogTimerId);
    }

    //
    std::list<std::string>::const_iterator it = mHandlers.begin();
    for (; it != mHandlers.end(); ++it)
    {
        if (!mIpcService->unregisterHandler(*it))
        {
            AI_LOG_ERROR( "failed to unregister '%s'", it->c_str());
        }
    }
    mHandlers.clear();

    // ensure any queue method or signal handlers are executed before returning
    mIpcService->flush();

    // rear down the manager and other components
    mManager.reset();
    mUtilities.reset();
    mEnvironment.reset();

    // shutdown the work queue
    mWorkQueue.reset();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Signal handler for SIGTERM
 *
 *
 */
void Dobby::sigTermHandler(int sigNum)
{
    if (sigNum == SIGTERM)
    {
        mSigTerm = 1;
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Signal handler that does nothing
 *
 *  This is needed to ensure SIGCHLD signals are actually delivered and
 *  detected by sigwaitinfo
 */
void Dobby::nullSigChildHandler(int sigNum, siginfo_t *info, void *context)
{
    (void)sigNum;
    (void)info;
    (void)context;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Utility function that MUST be called at startup from the main thread
 *  before any other threads are spawned.
 *
 *  This is needed to fix a bunch of quirks relating to how signals are handled,
 *  in particular the SIGCHLD signal.
 *
 *
 */
void Dobby::configSignals()
{
    AI_LOG_FN_ENTRY();

#ifdef USE_BREAKPAD
    // Breakpad will handle SIGILL, SIGABRT, SIGFPE and SIGSEGV
    AI_LOG_INFO("Breakpad support enabled");
    breakpad_ExceptionHandler();
#else
    AI_LOG_INFO("Breakpad support disabled");
#endif

    // Ignore SIGPIPE signal - the most annoying signal in the world
    signal(SIGPIPE, SIG_IGN);

    // Mask out SIGCHLD
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    sigprocmask(SIG_BLOCK, &mask, nullptr);
    pthread_sigmask(SIG_BLOCK, &mask, nullptr);

    // By default, SIGCHLD is set to be ignored so unless we happen to be
    // blocked on sigwaitinfo() at the time that SIGCHLD is set on us we will
    // not get it.  To fix this, we simply register a signal handler.  Since
    // we've masked the signal above, it will not affect us.  At the same time
    // we will make it a queued signal so that if more than one are set on us,
    // sigwaitinfo() will get them all.
    struct sigaction action;
    bzero(&action, sizeof(action));
    action.sa_sigaction = nullSigChildHandler;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);

    sigaction(SIGCHLD, &action, NULL);


    // Lastly install a signal handler for SIGTERM so that we can cleanly
    // shutdown from upstart which issues a SIGTERM to terminate the daemon
    bzero(&action, sizeof(action));
    action.sa_handler = sigTermHandler;
    action.sa_flags = SA_RESTART;
    sigemptyset(&action.sa_mask);

    sigaction(SIGTERM, &action, NULL);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Writes logging output to the console.
 *
 *  This duplicates code in the Logging component, but unfortunately we can't
 *  use the function there without messing up the API for all other things
 *  that use it.
 *
 */
void Dobby::logConsolePrinter(int level, const char *file, const char *func,
                              int line, const char *message)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);

    struct iovec iov[6];
    char tbuf[32];

    iov[0].iov_base = tbuf;
    iov[0].iov_len = snprintf(tbuf, sizeof(tbuf), "%.010lu.%.06lu ",
                              ts.tv_sec, ts.tv_nsec / 1000);
    iov[0].iov_len = std::min<size_t>(iov[0].iov_len, sizeof(tbuf));


    char threadbuf[32];
    iov[1].iov_base = threadbuf;
    iov[1].iov_len = snprintf(threadbuf, sizeof(threadbuf), "<T-%lu> ", syscall(SYS_gettid));
    iov[1].iov_len = std::min<size_t>(iov[1].iov_len, sizeof(threadbuf));

    switch (level)
    {
        case AI_DEBUG_LEVEL_FATAL:
            iov[2].iov_base = (void*)"FTL: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_ERROR:
            iov[2].iov_base = (void*)"ERR: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_WARNING:
            iov[2].iov_base = (void*)"WRN: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_MILESTONE:
            iov[2].iov_base = (void*)"MIL: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_INFO:
            iov[2].iov_base = (void*)"NFO: ";
            iov[2].iov_len = 5;
            break;
        case AI_DEBUG_LEVEL_DEBUG:
            iov[2].iov_base = (void*)"DBG: ";
            iov[2].iov_len = 5;
            break;
        default:
            iov[2].iov_base = (void*)": ";
            iov[2].iov_len = 2;
            break;
    }

    char fbuf[160];
    iov[3].iov_base = (void*)fbuf;
    if (!file || !func || (line <= 0))
        iov[3].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:? F:? L:? > ");
    else
        iov[3].iov_len = snprintf(fbuf, sizeof(fbuf), "< M:%.*s F:%.*s L:%d > ",
                                  64, file, 64, func, line);
    iov[3].iov_len = std::min<size_t>(iov[3].iov_len, sizeof(fbuf));

    iov[4].iov_base = const_cast<char*>(message);
    iov[4].iov_len = strlen(message);

    iov[5].iov_base = (void*)"\n";
    iov[5].iov_len = 1;


    writev(fileno((level < AI_DEBUG_LEVEL_INFO) ? stderr : stdout), iov, 6);
}

#if defined(RDK) && defined(USE_SYSTEMD)
// -----------------------------------------------------------------------------
/**
 *  @brief Writes logging output to systemd / journald
 *
 *
 *
 */
void Dobby::logJournaldPrinter(int level, const char *file, const char *func,
                               int line, const char *message)
{
    int priority;
    switch (level)
    {
        case AI_DEBUG_LEVEL_FATAL:          priority = LOG_CRIT;      break;
        case AI_DEBUG_LEVEL_ERROR:          priority = LOG_ERR;       break;
        case AI_DEBUG_LEVEL_WARNING:        priority = LOG_WARNING;   break;
        case AI_DEBUG_LEVEL_MILESTONE:      priority = LOG_NOTICE;    break;
        case AI_DEBUG_LEVEL_INFO:           priority = LOG_INFO;      break;
        case AI_DEBUG_LEVEL_DEBUG:          priority = LOG_DEBUG;     break;
        default:
            return;
    }

    std::string logLevel;
     switch (level)
    {
        case AI_DEBUG_LEVEL_FATAL:
            logLevel = "FTL: ";
            break;
        case AI_DEBUG_LEVEL_ERROR:
            logLevel = "ERR: ";
            break;
        case AI_DEBUG_LEVEL_WARNING:
            logLevel = "WRN: ";
            break;
        case AI_DEBUG_LEVEL_MILESTONE:
            logLevel = "MIL: ";
            break;
        case AI_DEBUG_LEVEL_INFO:
            logLevel = "NFO: ";
            break;
        case AI_DEBUG_LEVEL_DEBUG:
            logLevel = "DBG: ";
            break;
        default:
            logLevel = ": ";
            break;
    }

    sd_journal_send("SYSLOG_IDENTIFIER=DobbyDaemon",
                    "PRIORITY=%i", priority,
                    "CODE_FILE=%s", file,
                    "CODE_LINE=%i", line,
                    "CODE_FUNC=%s", func,
                    "MESSAGE=%s%s", logLevel.c_str(), message,
                    nullptr);
}
#endif // !defined(RDK)

// -----------------------------------------------------------------------------
/**
 *  @brief Logging callback, called every time a log message needs to be emitted
 *
 *  Depending on the log method, this will either send the message to syslog or
 *  the ethanlog library.
 *
 *  Note that this function is called after any processing in the Logger
 *  component, meaning that you can still print log messages on the console
 *  by using the AI_LOG_CHANNELS env var.
 */
void Dobby::logPrinter(int level, const char *file, const char *func,
                       int line, const char *message)
{
    if (mLogTargets & LogTarget::SysLog)
    {
        int priority;
        switch (level)
        {
            case AI_DEBUG_LEVEL_FATAL:          priority = LOG_CRIT;      break;
            case AI_DEBUG_LEVEL_ERROR:          priority = LOG_ERR;       break;
            case AI_DEBUG_LEVEL_WARNING:        priority = LOG_WARNING;   break;
            case AI_DEBUG_LEVEL_MILESTONE:      priority = LOG_NOTICE;    break;
            case AI_DEBUG_LEVEL_INFO:           priority = LOG_INFO;      break;
            case AI_DEBUG_LEVEL_DEBUG:          priority = LOG_DEBUG;     break;
            default:
                return;
        }

        syslog(priority, "< M:%s F:%s L:%d > %s", file, func, line, message);
    }

    if (mLogTargets & LogTarget::Console)
    {
        logConsolePrinter(level, file, func, line, message);
    }

#if defined(RDK) && defined(USE_SYSTEMD)
    if (mLogTargets & LogTarget::Journald)
    {
        logJournaldPrinter(level, file, func, line, message);
    }

#elif !defined(RDK)
    if (mLogTargets == LogTarget::EthanLog)
    {
        int _level;
        switch (level)
        {
            case AI_DEBUG_LEVEL_FATAL:      _level = ETHAN_LOG_FATAL;       break;
            case AI_DEBUG_LEVEL_ERROR:      _level = ETHAN_LOG_ERROR;       break;
            case AI_DEBUG_LEVEL_WARNING:    _level = ETHAN_LOG_WARNING;     break;
            case AI_DEBUG_LEVEL_MILESTONE:  _level = ETHAN_LOG_MILESTONE;   break;
            case AI_DEBUG_LEVEL_INFO:       _level = ETHAN_LOG_INFO;        break;
            case AI_DEBUG_LEVEL_DEBUG:      _level = ETHAN_LOG_DEBUG;       break;
            default:
                return;
        }

        ethanlog(_level, file, func, line, "%s", message);
    }

#endif // !defined(RDK)

}

// -----------------------------------------------------------------------------
/**
 *  @brief Static method must be called early in the startup before object
 *  is instantiated
 *
 *  If the function detects the ETHAN_LOGGING_PIPE env var then it sets the
 *  default logging method to 'ethan log', otherwise it defaults to syslog.
 *
 *
 */
void Dobby::setupLogging(unsigned targets /*= LogTarget::Console*/)
{
    // always setup syslog in-case the user wants to switch to it
    openlog("DobbyDaemon", 0, LOG_DAEMON);

    // set the default log targets
    mLogTargets = targets;

#if !defined(RDK)

    // we use the AI logging code in the Logger component, which by default
    // logs to stdout/stderr.  This is no use to us if we're running as a daemon
    // so instead check if the logging pipe is set in the env and if so redirect
    // output there.
    const char *env = getenv("ETHAN_LOGGING_PIPE");
    if (env)
    {
        mEthanLogPipeFd = atoi(env);
        mLogTargets |= LogTarget::EthanLog;
    }
    else
    {
        // the ethanlog library expects an environment var called
        // ETHAN_LOGGING_PIPE which contains the fd of the pipe to write to ...
        // because we may want to switch this on/off in the future we create an
        // fd now (to /dev/null) and set the env var to match it, then we can
        // just dup over the top of it to turn it on/off.
        mEthanLogPipeFd = open("/dev/null", O_CLOEXEC | O_WRONLY);

        char buf[32];
        sprintf(buf, "%d", mEthanLogPipeFd);
        setenv("ETHAN_LOGGING_PIPE", buf, 1);
    }

#endif

    AICommon::diag_printer_t printer = std::bind(logPrinter,
                                                 std::placeholders::_1,
                                                 std::placeholders::_2,
                                                 std::placeholders::_3,
                                                 std::placeholders::_4,
                                                 std::placeholders::_5);

    // initialise the actual logging code
    AICommon::initLogging(printer);
}


// -----------------------------------------------------------------------------
/**
 *  @brief Runs the Dobby work queue to handle API calls
 */
void Dobby::runWorkQueue() const
{
    while (!mShutdown)
    {
        // run the event loop for 500ms, this is so we can poll on the SIGTERM
        // signal monitor
        mWorkQueue->runFor(std::chrono::milliseconds(500));

        // check for SIGTERM
        if (mSigTerm != 0)
        {
            AI_LOG_INFO("detected SIGTERM, terminating daemon");
            break;
        }
    }

    return;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Issues a 'ready' signal over dbus and then blocks until either
 *  a shutdown request is received or SIGTERM
 *
 *
 *
 *
 */
void Dobby::run() const
{
    AI_LOG_FN_ENTRY();

    // send a signal out over dbus letting everyone know we're ready to start
    // processing requests
    const AI_IPC::Signal readySignal(mObjectPath,
                                     DOBBY_ADMIN_INTERFACE,
                                     DOBBY_ADMIN_EVENT_READY);
    if (!mIpcService->emitSignal(readySignal, { }))
    {
        AI_LOG_ERROR("failed to emit 'ready' signal");
    }

#if USE_SYSTEMD
    int ret = sd_notify(0, "READY=1");
    if (ret < 0)
    {
        AI_LOG_WARN("Failed to notify systemd we're ready");
    }
#endif

    // run the work event loop
    runWorkQueue();

    // Event loop is finished, we're shutting down
#if USE_SYSTEMD
    ret = sd_notify(0, "STOPPING=1");
    if (ret < 0)
    {
        AI_LOG_WARN("Failed to notify systemd we're stopping");
    }
#endif

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging function for manually setting the AI dbus addresses
 *
 *
 *  @param[in]  aiPrivateBusAddress     The AI private dbus address
 *  @param[in]  aiPublicBusAddress      The AI public dbus address
 *
 */
void Dobby::setDefaultAIDbusAddresses(const std::string& aiPrivateBusAddress,
                                      const std::string& aiPublicBusAddress)
{
    if (mIPCUtilities && !aiPrivateBusAddress.empty())
        mIPCUtilities->setAIDbusAddress(true, aiPrivateBusAddress);

    if (mIPCUtilities && !aiPublicBusAddress.empty())
        mIPCUtilities->setAIDbusAddress(false, aiPublicBusAddress);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Installs handlers for all the dbus/ipc methods
 *
 *
 *
 *
 */
void Dobby::initIpcMethods()
{
    AI_LOG_FN_ENTRY();

    // Table of all the methods ...
    const struct {
        const char *iface;
        const char *name;
        void (Dobby::*func)(std::shared_ptr<AI_IPC::IAsyncReplySender>);
    } methods[] = {

        {   DOBBY_ADMIN_INTERFACE,       DOBBY_ADMIN_METHOD_PING,                   &Dobby::ping                   },
        {   DOBBY_ADMIN_INTERFACE,       DOBBY_ADMIN_METHOD_SHUTDOWN,               &Dobby::shutdown               },
        {   DOBBY_ADMIN_INTERFACE,       DOBBY_ADMIN_METHOD_SET_LOG_METHOD,         &Dobby::setLogMethod           },
        {   DOBBY_ADMIN_INTERFACE,       DOBBY_ADMIN_METHOD_SET_LOG_LEVEL,          &Dobby::setLogLevel            },
        {   DOBBY_ADMIN_INTERFACE,       DOBBY_ADMIN_METHOD_SET_AI_DBUS_ADDR,       &Dobby::setAIDbusAddress       },

#if defined(LEGACY_COMPONENTS)
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_START,                   &Dobby::startFromSpec          },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_START_FROM_SPEC,         &Dobby::startFromSpec          },
#else
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_START,                   &Dobby::startFromBundle        },
#endif //defined(LEGACY_COMPONENTS)

        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_START_FROM_BUNDLE,       &Dobby::startFromBundle        },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_STOP,                    &Dobby::stop                   },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_PAUSE,                   &Dobby::pause                  },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_RESUME,                  &Dobby::resume                 },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_EXEC,                    &Dobby::exec                   },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_GETSTATE,                &Dobby::getState               },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_GETINFO,                 &Dobby::getInfo                },
        {   DOBBY_CTRL_INTERFACE,        DOBBY_CTRL_METHOD_LIST,                    &Dobby::list                   },

#if (AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)
        {   DOBBY_DEBUG_INTERFACE,       DOBBY_DEBUG_METHOD_CREATE_BUNDLE,          &Dobby::createBundle           },
        {   DOBBY_DEBUG_INTERFACE,       DOBBY_DEBUG_METHOD_GET_SPEC,               &Dobby::getSpec                },
#endif //(AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)

#if (AI_BUILD_TYPE == AI_DEBUG)
        {   DOBBY_DEBUG_INTERFACE,       DOBBY_DEBUG_METHOD_GET_OCI_CONFIG,         &Dobby::getOCIConfig           },
#endif // (AI_BUILD_TYPE == AI_DEBUG)

#if defined(AI_ENABLE_TRACING)
        {   DOBBY_DEBUG_INTERFACE,       DOBBY_DEBUG_START_INPROCESS_TRACING,       &Dobby::startInProcessTracing  },
        {   DOBBY_DEBUG_INTERFACE,       DOBBY_DEBUG_STOP_INPROCESS_TRACING,        &Dobby::stopInProcessTracing   },
#endif // defined(AI_ENABLE_TRACING)
    };

    // ... register them all
    for (const auto &method : methods)
    {
        std::string methodId =
            mIpcService->registerMethodHandler(AI_IPC::Method(mService, mObjectPath, method.iface, method.name),
                                               std::bind(method.func, this, std::placeholders::_1));
        if (methodId.empty())
        {
            AI_LOG_ERROR("failed to register '%s' method", method.name);
        }
        else
        {
            mHandlers.push_back(methodId);
        }
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Simple ping dbus method call
 *
 *
 *
 */
void Dobby::ping(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Not expecting any arguments
    // Drop Ping() log messages down to debug so we can run Dobby at INFO level
    // logging without spamming the log
    AI_LOG_DEBUG(DOBBY_ADMIN_METHOD_PING "()");

    // Send an empty pong reply back
    AI_IPC::VariantList results;
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send pong");
    }

    // If running as systemd service then also use this to wag the dog
#if defined(RDK) && defined(USE_SYSTEMD)
    mWorkQueue->postWork(
        [this]()
        {
            if (mWatchdogTimerId >= 0)
            {
                int ret = sd_notify(0, "WATCHDOG=1");
                if (ret < 0)
                {
                    AI_LOG_SYS_ERROR(-ret, "failed to send watchdog notification");
                }
            }
        }
    );
#endif

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Method called from admin client requesting the daemon to shutdown
 *
 *  This method unblocks the run() function.
 *
 *
 */
void Dobby::shutdown(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Not expecting any arguments
    AI_LOG_INFO(DOBBY_ADMIN_METHOD_SHUTDOWN "()");

    mShutdown = true;
    mWorkQueue->exit();

    // Send an empty reply back
    AI_IPC::VariantList results;
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Method called from APP_Process telling which method to use for logging
 *
 *  This method is provided with a single of mandatory fields; logMethod. An
 *  optional second parameter containing the logging pipe fd should be supplied
 *  if the log method is 'ethanlog'
 *
 *  @param[in]  replySender     Contains the arguments and the reply object.
 */
void Dobby::setLogMethod(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // be positive
    bool success = true;

    const AI_IPC::VariantList args = replySender->getMethodCallArguments();
    if (args.size() < 2)
    {
        AI_LOG_ERROR("invalid number of args");
        success = false;
    }
    else
    {
        try
        {
            uint32_t logMethod = boost::get<uint32_t>(args.at(0));

            AI_LOG_INFO(DOBBY_ADMIN_METHOD_SET_LOG_METHOD "(%u, ?)", logMethod);

            // if the method was 'ethanlog' then we expect the 2nd argument to
            // be an fd to the logging pipe
            if (logMethod == DOBBY_LOG_ETHANLOG)
            {
                AI_IPC::UnixFd logPipeFd = boost::get<AI_IPC::UnixFd>(args.at(1));
                if (!logPipeFd.isValid())
                {
                    AI_LOG_ERROR("received invalid log pipe fd over dbus");
                }
                else
                {
                    // replace the existing logging pipe fd with the new one
                    mEthanLogPipeFd = dup3(logPipeFd.fd(), mEthanLogPipeFd, O_CLOEXEC);
                }
            }

            // set the log globals
            unsigned newLogTarget;
            switch (logMethod)
            {
                case DOBBY_LOG_SYSLOG:
                    newLogTarget = LogTarget::SysLog;
                    break;
                case DOBBY_LOG_ETHANLOG:
                    newLogTarget = LogTarget::EthanLog;
                    break;
                case DOBBY_LOG_CONSOLE:
                    newLogTarget = LogTarget::Console;
                    break;
                case DOBBY_LOG_NULL:
                    newLogTarget = 0;
                    break;
                default:
                    AI_LOG_ERROR("invalid log type");
                    newLogTarget = 0;
                    success = false;
                    break;
            }

            if (success && (mLogTargets != newLogTarget))
            {
                // before switching send a 'log off' message
                AI_LOG_MILESTONE("logging switching to %s",
                                 (newLogTarget == LogTarget::SysLog)   ? "syslog"    :
                                 (newLogTarget == LogTarget::EthanLog) ? "diag"      :
                                 (newLogTarget == LogTarget::Console)  ? "console"   :
                                 (newLogTarget == LogTarget::Journald) ? "journald"  :
                                 (newLogTarget == 0)                   ? "/dev/null" : "ERR");

                mLogTargets = newLogTarget;
            }
        }
        catch (const boost::bad_get& e)
        {
            AI_LOG_ERROR("boost::bad_get exception '%s'", e.what());
            success = false;
        }
        catch (const std::exception& e)
        {
            AI_LOG_ERROR("std::exception exception '%s'", e.what());
            success = false;
        }
    }

    // Fire off the reply
    AI_IPC::VariantList results = { success };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Method called from APP_Process telling the log level to use.
 *
 *  The log level can only be dynamically changed on non-production builds.
 *
 *  @param[in]  replySender     Contains the arguments and the reply object.
 */
void Dobby::setLogLevel(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    bool result = false;

    // Expecting one args: (int32_t logLevel)
    int32_t logLevel;
    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &logLevel))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_ADMIN_METHOD_SET_LOG_LEVEL "(%d)", logLevel);

        __ai_debug_log_level = static_cast<int>(logLevel);
        result = true;
    }

    // Fire off the reply
    AI_IPC::VariantList results = { result };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Method called from APP_Process telling us the AI dbus addresses
 *
 *  The AI dbus addresses are stored internally and served up to any plugin
 *  that wants to talk over those buses.
 *
 *
 */
void Dobby::setAIDbusAddress(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    bool result = false;

    // Expecting two args:  (bool privateBus, string address)
    bool privateBus;
    std::string address;
    if (!AI_IPC::parseVariantList
            <bool, std::string>
            (replySender->getMethodCallArguments(), &privateBus, &address))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_ADMIN_METHOD_SET_AI_DBUS_ADDR "(%s, '%s')",
                    privateBus ? "true" : "false", address.c_str());

        result = mIPCUtilities->setAIDbusAddress(privateBus, address);
    }

    // Fire off the reply
    AI_IPC::VariantList results = { result };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

#if defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Starts a new container from the supplied json spec document.
 *
 *
 *
 *
 */
void Dobby::startFromSpec(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting 3/6 args:
    // (string id, string jsonSpec, vector<unixfd> files)
    // (string id, string jsonSpec, vector<unixfd> files, string command, string displaySocket, vector<string> envVars)
    std::string id;
    std::string jsonSpec;
    std::vector<AI_IPC::UnixFd> files;
    std::string command;
    std::string displaySocket;
    std::vector<std::string> envVars;

    // The command argument might not be sent at all (e.g. from AI) so we should
    // be able to withstand receiving 3 or 6 arguments
    bool parseArgsSuccess = false;
    if (replySender->getMethodCallArguments().size() == 3)
    {
        parseArgsSuccess = AI_IPC::parseVariantList<std::string,
                                                    std::string,
                                                    std::vector<AI_IPC::UnixFd>>(
            replySender->getMethodCallArguments(), &id, &jsonSpec, &files);
    }
    else if (replySender->getMethodCallArguments().size() == 6)
    {
        parseArgsSuccess = AI_IPC::parseVariantList<std::string,
                                                    std::string,
                                                    std::vector<AI_IPC::UnixFd>,
                                                    std::string,
                                                    std::string,
                                                    std::vector<std::string>>(
            replySender->getMethodCallArguments(), &id, &jsonSpec, &files, &command, &displaySocket, &envVars);
    }

    if (!parseArgsSuccess)
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_START_FROM_SPEC "('%s', ..., ...)", id.c_str());

        ContainerId id_ = ContainerId::create(id);
        if (!id_.isValid())
        {
            AI_LOG_ERROR("invalid container id '%s'", id.c_str());
        }
        else
        {
            // Try and start the container on the work queue thread
            auto doStartFromSpecLambda =
                [manager = mManager,
                 id = std::move(id_),
                 spec = std::move(jsonSpec),
                 files = std::move(files),
                 command = std::move(command),
                 displaySocket = std::move(displaySocket),
                 envVars = std::move(envVars),
                 replySender]()
                {
                    // Convert the vector of AI_IPC::UnixFd to a list of plain
                    // old integer file descriptors
                    std::list<int> fileList;
                    for (const AI_IPC::UnixFd &file : files)
                        fileList.push_back(file.fd());

                    // try and start the container
                    int32_t descriptor = manager->startContainerFromSpec(id, spec, fileList, command, displaySocket, envVars);

                    // Fire off the reply
                    AI_IPC::VariantList results = { descriptor };
                    if (!replySender->sendReply(results))
                    {
                        AI_LOG_ERROR("failed to send reply");
                    }
                };

            // Queue the work, if successful then we're done
            if (mWorkQueue->postWork(std::move(doStartFromSpecLambda)))
            {
                AI_LOG_FN_EXIT();
                return;
            }
        }
    }

    // Fire off an error reply
    AI_IPC::VariantList results = { int32_t(-1) };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}
#endif //defined(LEGACY_COMPONENTS)

// -----------------------------------------------------------------------------
/**
 *  @brief Starts a new container from the supplied bundle path.
 *
 *
 *
 *
 */
void Dobby::startFromBundle(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting 3/6 args:
    // (string id, string bundlePath, vector<unixfd> files)
    // (string id, string bundlePath, vector<unixfd> files, string command, string displaySocket, vector<string> envVars)
    std::string id;
    std::string bundlePath;
    std::vector<AI_IPC::UnixFd> files;
    std::string command;
    std::string displaySocket;
    std::vector<std::string> envVars;

    bool parseArgsSuccess = false;

    if (replySender->getMethodCallArguments().size() == 3)
    {
        parseArgsSuccess = AI_IPC::parseVariantList<std::string,
                                                    std::string,
                                                    std::vector<AI_IPC::UnixFd>>(
            replySender->getMethodCallArguments(), &id, &bundlePath, &files);
    }
    else if (replySender->getMethodCallArguments().size() == 6)
    {
        parseArgsSuccess = AI_IPC::parseVariantList<std::string,
                                                    std::string,
                                                    std::vector<AI_IPC::UnixFd>,
                                                    std::string,
                                                    std::string,
                                                    std::vector<std::string>>(
            replySender->getMethodCallArguments(), &id, &bundlePath, &files, &command, &displaySocket, &envVars);
    }

    if (!parseArgsSuccess)
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_START_FROM_BUNDLE "('%s', ..., ...)", id.c_str());

        ContainerId id_ = ContainerId::create(id);
        if (!id_.isValid())
        {
            AI_LOG_ERROR("invalid container id '%s'", id.c_str());
        }
        else
        {
            // Try and start the container on the work queue thread
            auto doStartFromBundleLambda =
                [manager = mManager,
                 id = std::move(id_),
                 path = std::move(bundlePath),
                 files = std::move(files),
                 command = std::move(command),
                 displaySocket = std::move(displaySocket),
                 envVars = std::move(envVars),
                 replySender]()
                {
                    // Convert the vector of AI_IPC::UnixFd to a list of plain
                    // old integer file descriptors
                    std::list<int> fileList;
                    for (const AI_IPC::UnixFd &file : files)
                        fileList.push_back(file.fd());

                    // try and start the container
                    int32_t descriptor = manager->startContainerFromBundle(id, path, fileList, command, displaySocket, envVars);

                    // Fire off the reply
                    AI_IPC::VariantList results = { descriptor };
                    if (!replySender->sendReply(results))
                    {
                        AI_LOG_ERROR("failed to send reply");
                    }
                };

            // Queue the work, if successful then we're done
            if (mWorkQueue->postWork(std::move(doStartFromBundleLambda)))
            {
                AI_LOG_FN_EXIT();
                return;
            }
        }
    }

    // Fire off an error reply
    AI_IPC::VariantList results = { int32_t(-1) };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Stops a running container
 *
 *
 *
 *
 */
void Dobby::stop(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting two args:  (int32_t cd, bool force)
    int32_t descriptor;
    bool force;
    if (!AI_IPC::parseVariantList
            <int32_t, bool>
            (replySender->getMethodCallArguments(), &descriptor, &force))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_STOP "(%d, %s)", descriptor,
                    force ? "true" : "false");

        // Try and stop the container on the work queue thread
        auto doStopLambda =
            [manager = mManager, descriptor, force, replySender]()
            {
                // Try and stop the container
                bool result = manager->stopContainer(descriptor, force);

                // Fire off the reply
                replySender->sendReply({ result });
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doStopLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off the reply
    AI_IPC::VariantList results = { false };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}


// -----------------------------------------------------------------------------
/**
 *  @brief Pauses (freezes) a running container
 *
 *
 *
 *
 */
void Dobby::pause(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting one argument:  (int32_t cd)
    int32_t descriptor;
    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &descriptor))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_PAUSE "(%d)", descriptor);

        // Try and pause the container on the work queue thread
        auto doPauseLambda =
            [manager = mManager, descriptor, replySender]()
            {
                // Try and pause the container
                bool result = manager->pauseContainer(descriptor);

                // Fire off the reply
                replySender->sendReply({ result });
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doPauseLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off the reply
    AI_IPC::VariantList results = { false };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Resumes a paused (frozen) container
 *
 *
 *
 *
 */
void Dobby::resume(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting one argument:  (int32_t cd)
    int32_t descriptor;

    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &descriptor))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_RESUME "(%d)", descriptor);

        auto doResumeLambda =
            [manager = mManager, descriptor, replySender]()
            {
                // Try and resume the container
                bool result = manager->resumeContainer(descriptor);

                // Fire off the reply
                if (!replySender->sendReply({ result }))
                {
                    AI_LOG_ERROR("failed to send reply");
                }
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doResumeLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off the reply
    AI_IPC::VariantList results = { false };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Executes a command in a container
 *
 *
 *
 *
 */
void Dobby::exec(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting three args:  (int32_t cd, string command, string options)
    int32_t descriptor;
    std::string command;
    std::string options;

    if (!AI_IPC::parseVariantList
            <int32_t, std::string, std::string>
            (replySender->getMethodCallArguments(), &descriptor, &options, &command))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_EXEC "(%d)", descriptor);

        auto doExecLambda =
            [manager = mManager,
             descriptor,
             options,
             command,
             replySender]()
            {
                // Try and execute the command
                bool result = manager->execInContainer(descriptor, options, command);

                // Fire off the reply
                if (!replySender->sendReply({ result }))
                {
                    AI_LOG_ERROR("failed to send reply");
                }
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doExecLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off the reply
    AI_IPC::VariantList results = { false };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets the state of a container
 *
 *
 *
 *
 */
void Dobby::getState(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting a single arg:  (int32_t cd)
    int32_t descriptor;
    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &descriptor))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_GETSTATE "('%d')", descriptor);

        // Get the state of the container
        auto doGetStateLambda =
            [manager = mManager, descriptor, replySender]()
            {
                // Try and stop the container
                int32_t result = manager->stateOfContainer(descriptor);

                // Fire off the reply
                replySender->sendReply({ result });
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doGetStateLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off an error reply
    if (!replySender->sendReply({ int32_t(-1) }))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Gets some info about a container
 *
 *  This is primarily a debugging method, used to get statistics on the
 *  container and roughly correlates to the 'runc events --stats <id>' call.
 *
 *  The reply is a json formatted string containing some info, it's form may
 *  change over time.
 *
 *      {
 *          "id": "blah",
 *          "state": "running",
 *          "timestamp": 348134887768,
 *          "pids": [ 1234, 1245 ],
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
 */
void Dobby::getInfo(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting a single arg:  (int32_t cd)
    int32_t descriptor;
    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &descriptor))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_CTRL_METHOD_GETINFO "('%d')", descriptor);

        // Get the info of the container
        auto doGetInfoLambda =
            [manager = mManager, descriptor, replySender]()
            {
                // Try and get container stats
                std::string result = manager->statsOfContainer(descriptor);

                // Fire off the reply
                replySender->sendReply({ result });
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doGetInfoLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off an error reply
    replySender->sendReply({ "" });

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Lists all the running containers
 *
 *  Method that returns a list of all the running container's descriptor and
 *  id strings.
 *
 *
 */
void Dobby::list(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Not expecting any arguments
    AI_LOG_INFO(DOBBY_CTRL_METHOD_LIST "()");

    // Get a list of all running containers
    auto doListLambda =
        [manager = mManager, replySender]()
        {
            // Try and get container stats
            const std::list<std::pair<int32_t, ContainerId>> containers =
                manager->listContainers();

            // We need to split the descriptors and id list as they're different type
            std::vector<int32_t> cds(containers.size(), -1);
            std::vector<std::string> ids(containers.size());
            size_t n = 0;

            for (const std::pair<int32_t, ContainerId>& details : containers)
            {
                cds[n] = details.first;
                ids[n] = details.second.str();
                n++;
            }
            // Fire off the reply
            replySender->sendReply({ cds, ids });
        };

    // Queue the work, if successful then we're done
    if (mWorkQueue->postWork(std::move(doListLambda)))
    {
        AI_LOG_FN_EXIT();
        return;
    }

    // Fire off an error reply
    AI_IPC::VariantList results = { std::vector<int32_t>(),
                                    std::vector<std::string>() };
    if (!replySender->sendReply(results))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

#if (AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility that can be used to create a bundle based on
 *  a dobby spec file
 *
 *  This can be useful for debugging container issues, as it allows the daemon
 *  to create the bundle but not actually run it, and therefore it can be
 *  run manually from the command line.
 *
 */
void Dobby::createBundle(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    // Expecting a two args:  (string id, string jsonSpec)
    std::string id;
    std::string jsonSpec;
    if (!AI_IPC::parseVariantList
        <std::string, std::string>
        (replySender->getMethodCallArguments(), &id, &jsonSpec))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_DEBUG_METHOD_CREATE_BUNDLE "('%s', ...)", id.c_str());

        ContainerId id_ = ContainerId::create(id);
        if (!id_.isValid())
        {
            AI_LOG_ERROR("invalid container id '%s'", id.c_str());
        }
        else
        {
            // Create the bundle (rootfs and config.json)
            auto doCreateBundleLambda =
                [manager = mManager, id = std::move(id_), spec = std::move(jsonSpec), replySender]()
                {
                    // Try and get container stats
                    bool result = manager->createBundle(id, spec);

                    // Fire off the reply
                    replySender->sendReply({ result });
                };

            // Queue the work, if successful then we're done
            if (mWorkQueue->postWork(std::move(doCreateBundleLambda)))
            {
                AI_LOG_FN_EXIT();
                return;
            }
        }
    }

    // Fire back an error reply
    if (!replySender->sendReply({ false }))
    {
        AI_LOG_ERROR("failed to send reply");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility to retrieve the original spec file for a running
 *  container (i.e. like the 'virsh dumpxml' command).
 *
 *
 *
 */
void Dobby::getSpec(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting a single arg:  (int32_t cd)
    int32_t descriptor;
    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &descriptor))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_DEBUG_METHOD_GET_SPEC "('%d')", descriptor);

        // Get the container spec
        auto doCreateBundleLambda =
            [manager = mManager, descriptor, replySender]()
            {
                // Get the container spec
                std::string spec = manager->specOfContainer(descriptor);

                // Fire off the reply
                replySender->sendReply({ spec });
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doCreateBundleLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off an error reply
    if (!replySender->sendReply({ "" }))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}
#endif //(AI_BUILD_TYPE == AI_DEBUG) && defined(LEGACY_COMPONENTS)

#if (AI_BUILD_TYPE == AI_DEBUG)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility to retrieve the OCI config.json file for a running
 *  container (i.e. like the 'virsh dumpxml' command).
 *
 *
 *
 */
void Dobby::getOCIConfig(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting a single arg:  (int32_t cd)
    int32_t descriptor;
    if (!AI_IPC::parseVariantList
            <int32_t>
            (replySender->getMethodCallArguments(), &descriptor))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_DEBUG_METHOD_GET_OCI_CONFIG "('%d')", descriptor);

        // Get the container spec
        auto doCreateBundleLambda =
            [manager = mManager, descriptor, replySender]()
            {
                // Read the config.json
                std::string configJson = manager->ociConfigOfContainer(descriptor);

                // Fire off the reply
                replySender->sendReply({ configJson });
            };

        // Queue the work, if successful then we're done
        if (mWorkQueue->postWork(std::move(doCreateBundleLambda)))
        {
            AI_LOG_FN_EXIT();
            return;
        }
    }

    // Fire off an error reply
    if (!replySender->sendReply({ "" }))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}
#endif // (AI_BUILD_TYPE == AI_DEBUG)

#if defined(AI_ENABLE_TRACING)
// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility to start the Perfetto in-process trace to a file
 *  enabling the given trace categories.
 *
 *
 *
 */
void Dobby::startInProcessTracing(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Pessimist
    bool result = false;

    // Expecting two args: (UnixFd traceFile, string categoryFilter)
    // Expecting a single arg:  (int32_t cd)
    AI_IPC::UnixFd traceFileFd;
    std::string categoryFilter;
    if (!AI_IPC::parseVariantList
            <AI_IPC::UnixFd, std::string>
            (replySender->getMethodCallArguments(), &traceFileFd, &categoryFilter))
    {
        AI_LOG_ERROR("error getting the args");
    }
    else
    {
        AI_LOG_INFO(DOBBY_DEBUG_START_INPROCESS_TRACING "(%d, '%s')",
                    traceFileFd.fd(), categoryFilter.c_str());

        result = PerfettoTracing::startInProcessTracing(traceFileFd.fd(),
                                                        categoryFilter);
    }

    // Fire off a reply with boolean result
    if (!replySender->sendReply( { result }))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Debugging utility to stop the Perfetto in-process tracing.
 *
 *
 *
 */
void Dobby::stopInProcessTracing(std::shared_ptr<AI_IPC::IAsyncReplySender> replySender)
{
    AI_LOG_FN_ENTRY();

    // Expecting no arguments

    AI_LOG_INFO(DOBBY_DEBUG_STOP_INPROCESS_TRACING "()");

    PerfettoTracing::stopInProcessTracing();

    // Fire off empty reply
    if (!replySender->sendReply( { true }))
    {
        AI_LOG_ERROR("failed to send reply");
    }

    AI_LOG_FN_EXIT();
}

#endif // defined(AI_ENABLE_TRACING)

// -----------------------------------------------------------------------------
/**
 *  @brief Called by the DobbyManager code when a container has started
 *
 *  This is typically called from DobbyManager in response to a
 *  DobbyManager::onPostStartHook, so just beware it's recursive
 *  from the dbus thread POV.
 *
 *  @param[in]  cd          The container unique descriptor.
 *  @param[in]  id          The string id / name of the container.
 */
void Dobby::onContainerStarted(int32_t cd, const ContainerId& id)
{
    AI_LOG_FN_ENTRY();

    // Regardless of whether of the not the hooks executed successfully
    // we fire off a notification event indicating that the container
    // has stopped.
    if (!mIpcService->emitSignal(AI_IPC::Signal(mObjectPath,
                                                DOBBY_CTRL_INTERFACE,
                                                DOBBY_CTRL_EVENT_STARTED),
                                 { cd, id.str() }))
    {
        AI_LOG_ERROR("failed to emit '%s' signal",
                     DOBBY_CTRL_EVENT_STARTED);
    }

    AI_LOG_MILESTONE("container '%s'(%d) started", id.c_str(), cd);

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called by the DobbyManager code when a container has stopped
 *
 *  This is typically called from an internal thread in the DobbyManager
 *  so be careful of any threading issues.
 *
 *
 *  @param[in]  cd          The container unique descriptor.
 *  @param[in]  id          The string id / name of the container.
 *  @param[in]  status      The status result of the container's runc process.
 */
void Dobby::onContainerStopped(int32_t cd, const ContainerId& id, int status)
{
    AI_LOG_FN_ENTRY();

    // Regardless of whether of the not the hooks executed successfully
    // we fire off a notification event indicating that the container
    // has stopped.
    if (!mIpcService->emitSignal(AI_IPC::Signal(mObjectPath,
                                                DOBBY_CTRL_INTERFACE,
                                                DOBBY_CTRL_EVENT_STOPPED),
                                 { cd, id.str() }))
    {
        AI_LOG_ERROR("failed to emit '%s' signal",
                     DOBBY_CTRL_EVENT_STOPPED);
    }

    AI_LOG_MILESTONE("container '%s'(%d) stopped (status 0x%04x)", id.c_str(),
                     cd, status);

    AI_LOG_FN_EXIT();
}

#if defined(RDK) && defined(USE_SYSTEMD)
#define WATCHDOG_TIMEOUT_SEC 10L
#define WATCHDOG_UPDATE_SEC  (WATCHDOG_TIMEOUT_SEC/2)
#define HIGH_USAGE_TIME_SEC  120L

// -----------------------------------------------------------------------------
/**
 *  @brief This function should be run as a thread for wagging watchdog
 *
 *  As on some platforms we expirienced heavy load from unidentified source
 *  Dobby got shut down by the watchdog. It is hard to pinpoint which process
 *  is taking those resources, but it looks like this happens during boot-up.
 *  This function if run as a separate thread will work around the problem by
 *  creating high priority watchdog wagger for the time period where the
 *  issue exists. During this time there will be 2 concurrent wagging procedures,
 *  but this doesn't harm.
 *  We should delete this code when we find out the real offender.
 *
 */
void wagWatchdogHeavyLoad()
{
    struct timespec wag_period, remaining;
    int ping_count;

    // set the lowest priority of real time policy
    struct sched_param sp;
    sp.sched_priority = sched_get_priority_min(SCHED_RR);
    int ret;

    ret = sched_setscheduler(0, SCHED_RR, &sp);
    if (ret == -1) {
        AI_LOG_ERROR("Couldn't schedule real time priority for wagWatchdogHeavyLoad");
        return;
    }

    pthread_setname_np(pthread_self(), "DOBBY_WATCHDOG");

    for (ping_count = HIGH_USAGE_TIME_SEC/WATCHDOG_UPDATE_SEC; ping_count > 0; ping_count--)
    {
        wag_period.tv_sec = WATCHDOG_UPDATE_SEC;
        wag_period.tv_nsec = 0L;

        // wait for desired time
        while(nanosleep(&wag_period, &remaining) && errno==EINTR){
            wag_period=remaining;
        }

        // We probably shouldn't log fails inside here, as logger could be blocked.
        sd_notify(0, "WATCHDOG=1");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Starts a timer to ping ourselves over dbus to send a watchdog
 *  notification.
 *
 *  This checks if the systemd watchdog is enabled and if yes it starts a timer
 *  to send ping method calls back to ourselves periodically.  Upon receiving
 *  the ping the code will wag the dog.
 *
 */
void Dobby::initWatchdog()
{
    AI_LOG_FN_ENTRY();

    uint64_t usecTimeout;

    int ret = sd_watchdog_enabled(1, &usecTimeout);

    if (ret < 0)
    {
        AI_LOG_SYS_ERROR(-ret, "failed to get watchdog enabled state");
        return;
    }
    else if (ret == 0)
    {
        AI_LOG_WARN("Not enabling watchdog");
        return;
    }

    usecTimeout /= 4;

    AI_LOG_INFO("starting watchdog timer with period %" PRId64, usecTimeout);

    mWatchdogTimerId =
        mUtilities->startTimer(std::chrono::microseconds(usecTimeout),
                                false,
                                std::bind(&Dobby::onWatchdogTimer, this));

    // Run heavy load wagger thread
    std::thread wagger(wagWatchdogHeavyLoad);
    wagger.detach();

    AI_LOG_FN_EXIT();
}

// -----------------------------------------------------------------------------
/**
 *  @brief Called when the watchdog timer expires.
 *
 *  To check the dobby service is still running we call ourselves over dbus
 *  with a ping method call.  The method call handler will call the libsystemd
 *  function to wag the dog.
 *
 */
bool Dobby::onWatchdogTimer()
{
    const AI_IPC::Method pingMethod(DOBBY_SERVICE,
                                    DOBBY_OBJECT,
                                    DOBBY_ADMIN_INTERFACE,
                                    DOBBY_ADMIN_METHOD_PING);

    mIpcService->invokeMethod(pingMethod, { });

    return true;
}

#endif // defined(RDK) && defined(USE_SYSTEMD)
