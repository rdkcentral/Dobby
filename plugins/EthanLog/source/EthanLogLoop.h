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
 * File:   EthanLogLoop.h
 *
 */

#ifndef ETHANLOGLOOP_H
#define ETHANLOGLOOP_H

#include "ContainerId.h"

#include <systemd/sd-event.h>

#include <memory>
#include <string>
#include <thread>
#include <mutex>
#include <deque>
#include <list>


typedef struct sd_event_source sd_event_source;

class EthanLogClient;


class EthanLogLoop
{
public:
    EthanLogLoop();
    ~EthanLogLoop();

    int addClient(const ContainerId& id, const std::string &tag,
                  unsigned allowedLevels,
                  uint64_t rate = 0, uint64_t burstSize = 0);

    void setClientBasePid(const ContainerId& id, pid_t basePid);

private:
    void eventLoop();
    void wakeLoop();

    static int eventFdHandler(sd_event_source *source, int fd,
                              uint32_t revents, void *userData);


private:
    std::thread mThread;
    std::mutex mLock;
    int mEventFd;

    struct Event
    {
        enum Type { Terminate, AddClient, SetClientBasePid } type;

        ContainerId id;
        int pipeFd;
        pid_t basePid;
        std::string tag;
        unsigned allowedLevels;
        uint64_t rate;
        uint64_t burstSize;

    protected:
        explicit Event(Type type_)
            : type(type_), id(ContainerId::create("")), basePid(-1)
            , pipeFd(-1), allowedLevels(0), rate(0), burstSize(0)
        { }

        Event(Type type_, const ContainerId &id_, const std::string &name, int fd,
              unsigned levels, uint64_t rate, uint64_t burst)
            : type(type_), id(id_), basePid(-1), pipeFd(fd), tag(name)
            , allowedLevels(levels) , rate(rate), burstSize(burst)
        { }

        Event(Type type_, const ContainerId &id_, pid_t basePid_)
            : type(type_), id(id_), basePid(basePid_), pipeFd(-1), tag()
            , allowedLevels(0) , rate(0), burstSize(0)
        { }
    };

    struct TerminateEvent : Event
    {
        TerminateEvent()
            : Event(Terminate)
        { }
    };

    struct AddClientEvent : Event
    {
        AddClientEvent(const ContainerId &id, const std::string &name, int fd,
                       unsigned levels, uint64_t rate, uint64_t burst)
            : Event(AddClient, id, name, fd, levels, rate, burst)
        { }
    };

    struct SetClientBasePidEvent : Event
    {
        SetClientBasePidEvent(const ContainerId &id, pid_t basePid)
            : Event(SetClientBasePid, id, basePid)
        { }
    };

    std::deque<Event> mEvents;

    std::list< std::unique_ptr<EthanLogClient> > mClients;
};


#endif // ETHANLOGLOOP_H
