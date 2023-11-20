/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "DobbyLoggerMock.h"

DobbyLogger::DobbyLogger()
{
}

DobbyLogger::DobbyLogger(const std::shared_ptr<const IDobbySettings> &settings)
{
}

DobbyLogger::~DobbyLogger()
{
}

void DobbyLogger::setImpl(DobbyLoggerImpl* newImpl)
{
    impl = newImpl;
}

DobbyLogger* DobbyLogger::getInstance()
{
    static DobbyLogger* instance = nullptr;
    if (nullptr == instance)
    {
       instance = new DobbyLogger();
    }
    return instance;
}

bool DobbyLogger::StartContainerLogging(std::string containerId,pid_t runtimePid,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin)
{
   EXPECT_NE(impl, nullptr);

    return impl->StartContainerLogging(containerId,runtimePid,containerPid,loggingPlugin);
}

bool DobbyLogger::DumpBuffer(int bufferMemFd,pid_t containerPid,std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin)
{
   EXPECT_NE(impl, nullptr);

    return impl->DumpBuffer(bufferMemFd,containerPid,loggingPlugin);
}

