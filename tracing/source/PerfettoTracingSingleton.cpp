//
//  PerfettoTracingSingleton.cpp
//
//  Copyright © 2020 Sky UK. All rights reserved.
//
#include "PerfettoTracing.h"
#include "PerfettoTracingSingleton.h"
#include "Tracing.h"

#include <Logging.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


PerfettoTracingSingleton *PerfettoTracingSingleton::mInstance = nullptr;
pthread_rwlock_t PerfettoTracingSingleton::mInstanceLock = PTHREAD_RWLOCK_INITIALIZER;

// -----------------------------------------------------------------------------
/*!
    Get the singleton instance of the Perfetto tracing interface.

 */
PerfettoTracingSingleton* PerfettoTracingSingleton::instance()
{
    pthread_rwlock_rdlock(&mInstanceLock);

    if (!mInstance)
    {
        // need to upgrade to a write lock and then check the instance again
        // to be sure that someone else hasn't jumped in and allocated it while
        // we were unlocked.
        pthread_rwlock_unlock(&mInstanceLock);
        pthread_rwlock_wrlock(&mInstanceLock);

        if (!mInstance)
        {
            mInstance = new PerfettoTracingSingleton;
        }
    }

    pthread_rwlock_unlock(&mInstanceLock);

    return mInstance;
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Called at shutdown to free the singleton.

 */
void PerfettoTracingSingleton::cleanUp()
{
    pthread_rwlock_wrlock(&mInstanceLock);

    delete mInstance;

    pthread_rwlock_unlock(&mInstanceLock);
}


// -----------------------------------------------------------------------------
/*!
    \internal

    Constructs the singleton instance.

 */
PerfettoTracingSingleton::PerfettoTracingSingleton()
    : mInitialised(false)
    , mBackends(0)
    , mTraceFileFd(-1)
{
    atexit(PerfettoTracingSingleton::cleanUp);
}

// -----------------------------------------------------------------------------
/*!
    \internal

    Flushes the trace buffer and closes any trace files.

 */
PerfettoTracingSingleton::~PerfettoTracingSingleton()
{
    if (mInProcessSession)
        stopInProcessTracing();

    if ((mTraceFileFd >= 0) && (close(mTraceFileFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close trace file");
}

// -----------------------------------------------------------------------------
/*!
    Sets the tracing mode to either 'system' or 'in process'.

    This is a one time operation, it is not possible to change the mode once
    set.

 */
bool PerfettoTracingSingleton::initialise(unsigned int backends)
{
    if (backends == 0)
    {
        AI_LOG_ERROR("at least one backend must be enabled");
        return false;
    }

    std::lock_guard<std::mutex> locker(mLock);

    if (mInitialised)
    {
        AI_LOG_WARN("perfetto tracing already enabled");
        return true;
    }

    perfetto::TracingInitArgs args;

    // the backends determine where trace events are recorded.  We are going to
    // use the system-wide tracing service, so that we can see our app's events
    // in context with system profiling information
    args.backends = 0;
    if (backends & PerfettoTracing::SystemBackend)
    {
        args.backends |= perfetto::kSystemBackend;
    }
    if (backends & PerfettoTracing::InProcessBackend)
    {
        args.backends |= perfetto::kInProcessBackend;
    }

    perfetto::Tracing::Initialize(args);

    // register all the track events
    perfetto::TrackEvent::Register();

    // save the backends and initialised flag
    mBackends = backends;
    mInitialised = true;
}

// -----------------------------------------------------------------------------
/*!
    Starts an in process trace, writing the trace file to the given \a fd.

 */
bool PerfettoTracingSingleton::startInProcessTracing(int fd,
                                                     const std::string &categoryFilter)
{
    // FIXME: enable the category filter
    (void) categoryFilter;


    std::lock_guard<std::mutex> locker(mLock);

    if (!(mBackends & PerfettoTracing::InProcessBackend))
    {
        AI_LOG_ERROR("in process tracing backend not enabled");
        return false;
    }

    if (mInProcessSession)
    {
        AI_LOG_ERROR("tracing session already running");
        return false;
    }

    // dup the supplied fd because perfetto doesn't
    mTraceFileFd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
    if (mTraceFileFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to dup trace file fd");
        return false;
    }

    // the trace config defines which types of data sources are enabled for
    // recording. We just need the "track_event" data source, which corresponds
    // to the TRACE_EVENT trace points.
    perfetto::TraceConfig config;
    config.add_buffers()->set_size_kb(1024);

    {
        auto *dataSourceConfig = config.add_data_sources()->mutable_config();
        dataSourceConfig->set_name("track_event");

        /* FIXME add categories
        perfetto::protos::gen::TrackEventConfig track_event_cfg;
        track_event_cfg.add_disabled_categories("*");
        track_event_cfg.add_enabled_categories("rendering");
        dataSourceConfig->set_track_event_config_raw(track_event_cfg.SerializeAsString());
         */
    }


    // start tracing
    mInProcessSession = perfetto::Tracing::NewTrace(perfetto::kInProcessBackend);
    if (!mInProcessSession)
    {
        AI_LOG_ERROR("failed to create new in-process tracing session");
        return false;
    }

    mInProcessSession->Setup(config, mTraceFileFd);
    mInProcessSession->StartBlocking();


    return true;
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if currently tracing.

    For in process tracing, this will return \c true if startInProcessTracing()
    was called.  For system tracing this will only return \c true if the system
    traced daemon has started the trace.

 */
bool PerfettoTracingSingleton::isTracing() const
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!mInitialised)
    {
        return false;
    }

    if (mInProcessSession)
    {
        // if mode is 'in process' then the session is only valid when a trace
        // is running
        return true;
    }

    // if in 'system' mode then we need to query the traced daemon to see
    // if our trace events are enabled
    bool started = false;
    perfetto::TrackEvent::CallIfEnabled([&](uint32_t) { started = true; });
    return started;
}

// -----------------------------------------------------------------------------
/*!
    Stops the 'in process' tracing.

 */
void PerfettoTracingSingleton::stopInProcessTracing()
{
    std::lock_guard<std::mutex> locker(mLock);

    if (!mInProcessSession)
    {
        AI_LOG_WARN("no 'in process' tracing session running");
        return;
    }

    // make sure everything is flushed to the target
    perfetto::TrackEvent::Flush();

    // stop tracing
    mInProcessSession->StopBlocking();
    mInProcessSession.reset();

    // close the trace file
    if ((mTraceFileFd >= 0) && (close(mTraceFileFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close trace file");
    }

    mTraceFileFd = -1;
}

