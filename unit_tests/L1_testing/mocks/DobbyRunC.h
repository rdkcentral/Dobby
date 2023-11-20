/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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
 * File:   DobbyRunc.h
 *
 */
#ifndef DOBBYRUNC_H
#define DOBBYRUNC_H

#include "IDobbyUtils.h"
#include "DobbyLogger.h"
#include "ContainerId.h"

#include <json/json.h>

#include <memory>
#include <mutex>
#include <list>

class DobbyBundle;
class IDobbyStream;

// -----------------------------------------------------------------------------

class DobbyRunCImpl;

class DobbyRunC
{
protected:
    static DobbyRunCImpl* impl;
public:
    DobbyRunC(const std::shared_ptr<IDobbyUtils> &utils,
                       const std::shared_ptr<const IDobbySettings> &settings);
    ~DobbyRunC();

public:
enum class ContainerStatus
{
    Unknown,
    Created,
    Running,
    Pausing,
    Paused,
    Stopped
};

struct ContainerListItem
{
    ContainerId id;
    pid_t pid;
    std::string bundlePath;
    ContainerStatus status;
};
public:
    DobbyRunC();
    static void setImpl(DobbyRunCImpl* newImpl);
    static DobbyRunC* getInstance();

    std::pair<pid_t, pid_t> create(const ContainerId &id,
                                   const std::shared_ptr<const DobbyBundle> &bundle,
                                   const std::shared_ptr<const IDobbyStream> &console,
                                   const std::list<int> &files = std::list<int>(),
                                   const std::string& customConfigPath = "") const;
    bool destroy(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force = false) const;
    bool start(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) const;
    bool killCont(const ContainerId &id, int signal, bool all = false) const;
    bool pause(const ContainerId &id) const;
    bool resume(const ContainerId &id) const;
    std::pair<pid_t, pid_t> exec(const ContainerId &id,
                                 const std::string &options,
                                 const std::string &command) const;
    ContainerStatus state(const ContainerId &id) const;
    std::list<ContainerListItem> list() const;
    const std::string getWorkingDir() const;
};

class DobbyRunCImpl
{
public:
    virtual std::pair<pid_t, pid_t> create(const ContainerId &id,
                                   const std::shared_ptr<const DobbyBundle> &bundle,
                                   const std::shared_ptr<const IDobbyStream> &console,
                                   const std::list<int> &files = std::list<int>(),
                                   const std::string& customConfigPath = "") const = 0;
    virtual bool destroy(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console, bool force = false) const = 0;
    virtual bool start(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) const = 0;
    virtual bool killCont(const ContainerId &id, int signal, bool all = false) const = 0;
    virtual bool pause(const ContainerId &id) const = 0;
    virtual bool resume(const ContainerId &id) const = 0;
    virtual std::pair<pid_t, pid_t> exec(const ContainerId &id,
                                 const std::string &options,
                                 const std::string &command) const = 0;

    virtual DobbyRunC::ContainerStatus state(const ContainerId &id) const = 0;
    virtual std::list<DobbyRunC::ContainerListItem> list() const = 0;
    virtual const std::string getWorkingDir() const = 0;
};

#endif // !defined(DOBBYRUNC_H)
