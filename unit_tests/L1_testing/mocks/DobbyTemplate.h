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

#ifndef DOBBYTEMPLATE_H
#define DOBBYTEMPLATE_H

#pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#  include <ctemplate/template.h>
#pragma GCC diagnostic pop

#include <pthread.h>

#include <memory>
#include <string>
#include <list>

class DobbyTemplateImpl {
public:

    virtual ~DobbyTemplateImpl() = default;
    virtual void setSettings(const std::shared_ptr<const IDobbySettings>& settings) const = 0;
    virtual std::string apply(const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint) = 0;
    virtual bool applyAt(int dirFd, const std::string& fileName, const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint) = 0;
};

class DobbyTemplate {

protected:

    static DobbyTemplateImpl* impl;

public:

    DobbyTemplate();
    static void setImpl(DobbyTemplateImpl* newImpl);
    static DobbyTemplate* getInstance();
    static void setSettings(const std::shared_ptr<const IDobbySettings>& settings);
    static std::string apply(const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint);
    static bool applyAt(int dirFd, const std::string& fileName, const ctemplate::TemplateDictionaryInterface* dictionary, bool prettyPrint);

  };
#endif // !defined(DOBBYTEMPLATE_H)
