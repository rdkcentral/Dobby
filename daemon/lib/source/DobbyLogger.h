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

#ifndef DOBBYLOGGER_H
#define DOBBYLOGGER_H

#include <sys/types.h>

#include "ContainerId.h"
#include "IDobbyRdkLoggingPlugin.h"
#include <IDobbySettings.h>
#include "DobbyRdkPluginManager.h"
#include "DobbyLogRelay.h"
#include "PollLoop.h"
#include <rt_dobby_schema.h>

#include <pthread.h>

#include <string>
#include <thread>
#include <vector>
#include <mutex>

class DobbyLogger
{
public:
    DobbyLogger(const std::shared_ptr<const IDobbySettings> &settings);
    ~DobbyLogger();

public:
    bool StartContainerLogging(std::string containerId,
                               pid_t runtimePid,
                               pid_t containerPid,
                               std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin,
                               const bool createNewLog);

    bool DumpBuffer(int bufferMemFd,
                    pid_t containerPid,
                    std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin,
                    const bool createNewLog);

private:
    int createDgramSocket(const std::string &path);
    int createUnixSocket(const std::string path);
    int receiveFdFromSocket(const int connectionFd);
    void connectionMonitorThread(const int socketFd);

private:
    std::mutex mLock;
    int mSocketFd;
    const std::string mSocketPath;
    const std::string mSyslogSocketPath;
    const std::string mJournaldSocketPath;

    int mSyslogFd;
    int mJournaldFd;

    std::map<pid_t, IDobbyRdkLoggingPlugin::LoggingOptions> mTempConnections;
    std::map<pid_t, std::future<void>> mFutures;

    bool mShutdown;

    std::shared_ptr<AICommon::PollLoop> mPollLoop;

    std::shared_ptr<DobbyLogRelay> mSyslogRelay;
    std::shared_ptr<DobbyLogRelay> mJournaldRelay;
};

#endif // !defined(DOBBYLOGGER_H)

