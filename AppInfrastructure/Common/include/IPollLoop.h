/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2015 Sky UK
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
 * File:   IPollLoop.h
 *
 */
#ifndef IPOLLLOOP_H
#define IPOLLLOOP_H

#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>

#include <memory>
#include <thread>

#define EPOLLDEFERRED  (1 << 29)

namespace AICommon {

class IPollLoop;

class IPollSource
{
public:
    virtual ~IPollSource() { }

public:
    virtual void process(const std::shared_ptr<IPollLoop>& pollLoop, epoll_event event) = 0;
};

class IPollLoop
{
public:
    virtual ~IPollLoop() { }

public:
    virtual bool start(int priority = -1) = 0;
    virtual void stop() = 0;

    virtual std::thread::id threadId() const = 0;
    virtual pid_t gettid() const = 0;

    virtual bool addSource(const std::shared_ptr<IPollSource>& source, int fd, uint32_t events) = 0;
    virtual bool modSource(const std::shared_ptr<IPollSource>& source, uint32_t events) = 0;
    virtual void delSource(const std::shared_ptr<IPollSource>& source, int fd = -1) = 0;
    virtual void delAllSources() = 0;
};

} // namespace AICommon

#endif // !defined(IPOLLLOOP_H)
