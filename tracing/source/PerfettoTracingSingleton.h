//
//  PerfettoTracingSingleton.h
//
//  Copyright © 2020 Sky UK. All rights reserved.
//
#ifndef PERFETTOTRACINGSINGLETON_H
#define PERFETTOTRACINGSINGLETON_H

#include <perfetto.h>

#include <pthread.h>

#include <string>
#include <memory>
#include <mutex>
#include <chrono>

// define an empty set of categories if tracing is disabled
#if !defined(AI_ENABLE_TRACING)
    PERFETTO_DEFINE_CATEGORIES();
#endif


class PerfettoTracingSingleton
{
private:
    static void cleanUp();
    static PerfettoTracingSingleton* mInstance;
    static pthread_rwlock_t mInstanceLock;

private:
    PerfettoTracingSingleton();

public:
    static PerfettoTracingSingleton* instance();
    ~PerfettoTracingSingleton();

    bool initialise(unsigned backends);

    bool isTracing() const;

    bool startInProcessTracing(int fd, const std::string &categoryFilter);
    void stopInProcessTracing();

private:
    mutable std::mutex mLock;
    bool mInitialised;
    unsigned mBackends;

    int mTraceFileFd;
    std::unique_ptr<perfetto::TracingSession> mInProcessSession;

};


#endif // PERFETTOTRACINGSINGLETON_H
