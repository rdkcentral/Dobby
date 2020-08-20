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
#ifndef PERFETTOTRACING_H
#define PERFETTOTRACING_H

#include <perfetto.h>
#include <string>


class PerfettoTracing
{
public:
    enum Backend : unsigned
    {
        SystemBackend = 0x1,
        InProcessBackend = 0x2,
    };

    static bool initialise(unsigned backends = (SystemBackend | InProcessBackend));

    static bool isTracing();

    static bool startInProcessTracing(const std::string &traceFile,
                                      const std::string &categoryFilter = std::string());
    static bool startInProcessTracing(int fd,
                                      const std::string &categoryFilter = std::string());
    static void stopInProcessTracing();
};

#endif