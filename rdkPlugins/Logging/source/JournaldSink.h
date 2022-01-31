/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2022 Sky UK
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

#pragma once

#include "ILoggingSink.h"

class JournaldSink : public ILoggingSink
{
public:
    JournaldSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig);
    ~JournaldSink();

public:
    void DumpLog(const int bufferFd) override;

    void SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions& options) override;

    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;

private:
    int mJournaldSteamFd;
    const std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mContainerId;
    IDobbyRdkLoggingPlugin::LoggingOptions mLoggingOptions;

    char mBuf[PTY_BUFFER_SIZE];

    std::mutex mLock;
};