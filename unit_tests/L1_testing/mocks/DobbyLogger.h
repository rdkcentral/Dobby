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
 * File:   DobbyLegacyPluginManager.h
 *
 */

#ifndef DOBBYLOGGER_H
#define DOBBYLOGGER_H

#include <sys/types.h>

#include "ContainerId.h"
#include "IDobbyRdkLoggingPlugin.h"
#include <IDobbySettings.h>

class DobbyLoggerImpl {
public:

    virtual ~DobbyLoggerImpl() = default;

    virtual bool StartContainerLogging(std::string containerId,pid_t runtimePid,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin) = 0;
    virtual bool DumpBuffer(int bufferMemFd,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin) = 0;

};

class DobbyLogger {

protected:
    static DobbyLoggerImpl* impl;

public:
    DobbyLogger();
    DobbyLogger(const std::shared_ptr<const IDobbySettings> &settings);
    ~DobbyLogger();

    static void setImpl(DobbyLoggerImpl* newImpl);
    static DobbyLogger* getInstance();
    static bool StartContainerLogging(std::string containerId,pid_t runtimePid,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin);
    static bool DumpBuffer(int bufferMemFd,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin);
};

#endif // !defined(DOBBYLOGGER_H)


