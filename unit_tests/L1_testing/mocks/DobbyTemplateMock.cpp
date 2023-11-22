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

#include "IDobbySettings.h"
#include "DobbyTemplateMock.h"

DobbyTemplate::DobbyTemplate()
{
}

void DobbyTemplate::setImpl(DobbyTemplateImpl* newImpl)
{
    impl = newImpl;
}

DobbyTemplate* DobbyTemplate::getInstance()
{
    static DobbyTemplate* instance = nullptr;
    if(nullptr == instance)
    {
       instance = new DobbyTemplate();
    }
    return instance;
}

void DobbyTemplate::setSettings(const std::shared_ptr<const IDobbySettings>& settings)
{
   EXPECT_NE(impl, nullptr);

    return impl->setSettings(settings);
}

std::string DobbyTemplate::apply(const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint)
{
   EXPECT_NE(impl, nullptr);

    return impl->apply(dictionary, prettyPrint);
}

bool DobbyTemplate::applyAt(int dirFd, const std::string& fileName, const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint)
{
   EXPECT_NE(impl, nullptr);

    return impl->applyAt(dirFd, fileName, dictionary, prettyPrint);
}
