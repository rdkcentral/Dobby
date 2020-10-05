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
//  PerfettoTracing.cpp
//

#include "Tracing.h"
#include "PerfettoTracing.h"
#include "PerfettoTracingSingleton.h"

#include <Logging.h>

#include <perfetto.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

// reserves internal static storage for our tracing categories.
PERFETTO_TRACK_EVENT_STATIC_STORAGE();


bool PerfettoTracing::initialise(unsigned backends)
{
    return PerfettoTracingSingleton::instance()->initialise(backends);
}

bool PerfettoTracing::isTracing()
{
    return PerfettoTracingSingleton::instance()->isTracing();
}

bool PerfettoTracing::startInProcessTracing(int fd,
                                            const std::string &categoryFilter)
{
    return PerfettoTracingSingleton::instance()->startInProcessTracing(fd, categoryFilter);
}

bool PerfettoTracing::startInProcessTracing(const std::string &traceFile,
                                            const std::string &categoryFilter)
{
    if (isTracing())
    {
        AI_LOG_WARN("trace already running");
        return false;
    }

    // open / create the trace file
    int fd = open(traceFile.c_str(), O_CLOEXEC | O_CREAT | O_TRUNC | O_RDWR, 0644);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open / create trace file @ '%s'",
                         traceFile.c_str());
        return false;
    }

    // start tracing to the file
    bool result = PerfettoTracingSingleton::instance()->startInProcessTracing(fd, categoryFilter);
    if (!result)
    {
        // delete the file if we failed to start the trace
        unlink(traceFile.c_str());
    }

    // close the trace file, as the instance will dup it if tracing started
    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close trace file");
    }

    return result;
}

void PerfettoTracing::stopInProcessTracing()
{
    PerfettoTracingSingleton::instance()->stopInProcessTracing();
}

