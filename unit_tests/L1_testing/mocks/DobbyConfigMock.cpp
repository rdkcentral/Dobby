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

#include <gmock/gmock.h>

#include "DobbyConfigMock.h"

void DobbyConfig::setImpl(DobbyConfigImpl* newImpl)
{
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

bool DobbyConfig::writeConfigJson(const std::string& filePath) const
{
   EXPECT_NE(impl, nullptr);

    return impl->writeConfigJson(filePath);
}

const std::shared_ptr<rt_dobby_schema> DobbyConfig::config()
{
   EXPECT_NE(impl, nullptr);

    return impl->config();
}

bool DobbyConfig::changeProcessArgs(const std::string& command) 
{
   EXPECT_NE(impl, nullptr);

    return impl->changeProcessArgs(command);
}

bool DobbyConfig::addWesterosMount(const std::string& socketPath) 
{
   EXPECT_NE(impl, nullptr);

    return impl->addWesterosMount(socketPath);
}

bool DobbyConfig::addEnvironmentVar(const std::string& envVar) 
{
   EXPECT_NE(impl, nullptr);

    return impl->addEnvironmentVar(envVar);
}

bool DobbyConfig::enableSTrace(const std::string& logsDir) 
{
   EXPECT_NE(impl, nullptr);

    return impl->enableSTrace(logsDir);
}

void DobbyConfig::setApparmorProfile(const std::string& profileName)
{
   EXPECT_NE(impl, nullptr);

    return impl->setApparmorProfile(profileName);
}

void DobbyConfig::setPidsLimit(int limit)
{
   EXPECT_NE(impl, nullptr);

    return impl->setPidsLimit(limit);
}

const std::string DobbyConfig::configJson() const
{
   EXPECT_NE(impl, nullptr);

    return impl->configJson();
}
