/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * Copyright (C) Sky UK 2016+
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
/**
 *  @class DobbyRunC
 *  @brief Wrapper around the runc command line app.
 *
 *  This class just formats the args and environment variables to give the
 *  runc command line app, which we launch with a fork/exec.
 *
 */
class DobbyRunC
{
public:
    explicit DobbyRunC(const std::shared_ptr<IDobbyUtils> &utils,
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

public:
    std::pair<pid_t, pid_t> create(const ContainerId &id,
                                   const std::shared_ptr<const DobbyBundle> &bundle,
                                   const std::shared_ptr<const IDobbyStream> &console,
                                   const std::list<int> &files = std::list<int>(),
                                   const std::string& customConfigPath = "") const;

    bool destroy(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) const;
    bool start(const ContainerId &id, const std::shared_ptr<const IDobbyStream> &console) const;
    bool kill(const ContainerId &id, int signal, bool all = false) const;
    bool pause(const ContainerId &id) const;
    bool resume(const ContainerId &id) const;
    std::pair<pid_t, pid_t> exec(const ContainerId &id,
                                 const std::string &options,
                                 const std::string &command) const;

    ContainerStatus state(const ContainerId &id) const;
    std::map<ContainerId, ContainerStatus> list() const;

public:
    pid_t run(const ContainerId &id,
              const std::shared_ptr<const DobbyBundle> &bundle,
              const std::shared_ptr<const IDobbyStream> &console,
              const std::list<int> &files = std::list<int>()) const;

private:
    pid_t forkExecRunC(const std::vector<const char *> &args,
                       const std::initializer_list<const char *> &envs,
                       const std::list<int> &files = std::list<int>(),
                       const std::shared_ptr<const IDobbyStream> &stdoutStream = nullptr,
                       const std::shared_ptr<const IDobbyStream> &stderrStream = nullptr) const;

    ContainerStatus getContainerStatusFromJson(const Json::Value &state) const;

private:
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::string mRuncPath;

    const std::string mWorkingDir;
    const std::string mLogDir;
    const std::string mLogFilePath;
    const std::string mConsoleSocket;
};

#endif // !defined(DOBBYRUNC_H)
