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

#include "DobbySpecConfigMock.h"

DobbySpecConfig::DobbySpecConfig()
{
}

DobbySpecConfig::DobbySpecConfig(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const IDobbySettings>& settings,const ContainerId& id,const std::shared_ptr<const DobbyBundle>& bundle,const std::string& specJson)
{
}

DobbySpecConfig::DobbySpecConfig(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const IDobbySettings>& settings,const std::shared_ptr<const DobbyBundle>& bundle,const std::string& specJson)
{
}

DobbySpecConfig::~DobbySpecConfig()
{
}

void DobbySpecConfig::setImpl(DobbySpecConfigImpl* newImpl)
{
    impl = newImpl;
}

DobbySpecConfig* DobbySpecConfig::getInstance()
{
    static DobbySpecConfig* instance = nullptr;
    if (nullptr == instance)
    {
       instance = new DobbySpecConfig();
    }
    return instance;
}

bool DobbySpecConfig::isValid()
{
   EXPECT_NE(impl, nullptr);

    return impl->isValid();
}

const std::map<std::string, Json::Value>& DobbySpecConfig::rdkPlugins()
{
   EXPECT_NE(impl, nullptr);

    return impl->rdkPlugins();
}

#if defined(LEGACY_COMPONENTS)

const std::string DobbySpecConfig::spec()
{
   EXPECT_NE(impl, nullptr);

    return impl->spec();
}
#endif //defined(LEGACY_COMPONENTS)

std::shared_ptr<rt_dobby_schema> DobbySpecConfig::config()
{
   EXPECT_NE(impl, nullptr);

    return impl->config();
}

bool DobbySpecConfig::restartOnCrash()
{
   EXPECT_NE(impl, nullptr);

    return impl->restartOnCrash();
}

bool DobbySpecConfig::writeConfigJson(const std::string& filePath)
{
   EXPECT_NE(impl, nullptr);

    return impl->writeConfigJson(filePath);
}

