/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
//  PerfettoTracingSingleton.h
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
