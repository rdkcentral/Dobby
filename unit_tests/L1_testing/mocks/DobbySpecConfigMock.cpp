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
    // Handles both resetting 'impl' to nullptr and assigning a new value to 'impl'
    EXPECT_TRUE ((nullptr == impl) || (nullptr == newImpl));
    impl = newImpl;
}

bool DobbySpecConfig::isValid() const
{
   EXPECT_NE(impl, nullptr);

    return impl->isValid();
}

const std::map<std::string, Json::Value>& DobbySpecConfig::rdkPlugins() const
{
   EXPECT_NE(impl, nullptr);

    return impl->rdkPlugins();
}

#if defined(LEGACY_COMPONENTS)

const std::string DobbySpecConfig::spec() const
{
   EXPECT_NE(impl, nullptr);

    return impl->spec();
}

const std::map<std::string, Json::Value>& DobbySpecConfig::legacyPlugins() const
{
   EXPECT_NE(impl, nullptr);
    return impl->legacyPlugins();
}

#endif //defined(LEGACY_COMPONENTS)

std::shared_ptr<rt_dobby_schema> DobbySpecConfig::config() const
{
   EXPECT_NE(impl, nullptr);

    return impl->config();
}

bool DobbySpecConfig::restartOnCrash() const
{
   EXPECT_NE(impl, nullptr);

    return impl->restartOnCrash();
}
