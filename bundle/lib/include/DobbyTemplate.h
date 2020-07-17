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
 * File:   DobbyTemplate.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef DOBBYTEMPLATE_H
#define DOBBYTEMPLATE_H

// Some ctemplate classes have a variable that triggers the -Wshadow gcc warning.
// However it is benign, therefore we temporary disable the warning here.
#pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#  include <ctemplate/template.h>
#pragma GCC diagnostic pop

#include <pthread.h>

#include <memory>
#include <string>
#include <list>


class IDobbySettings;


// -----------------------------------------------------------------------------
/**
 *  @class DobbyTemplate
 *  @brief Singleton class that returns the OCI JSON template.
 *
 *
 *
 */
class DobbyTemplate
{
private:
    DobbyTemplate();
    static void cleanUp();

private:
    const ctemplate::TemplateString mTemplateKey;
    const std::unique_ptr<ctemplate::TemplateCache> mTemplateCache;

    static DobbyTemplate* instance();

    void setTemplateDevNodes(const std::list<std::string>& devNodes);
    void setTemplateEnvVars(const std::map<std::string, std::string>& envVars);
    void setTemplatePlatformEnvVars();
    void setTemplateCpuRtSched();

    void _setSettings(const std::shared_ptr<const IDobbySettings>& settings);

    std::string _apply(const ctemplate::TemplateDictionaryInterface* dictionary,
                       bool prettyPrint) const;

    bool _applyAt(int dirFd, const std::string& fileName,
                  const ctemplate::TemplateDictionaryInterface* dictionary,
                  bool prettyPrint) const;

public:
    static void setSettings(const std::shared_ptr<const IDobbySettings>& settings);

    static std::string apply(const ctemplate::TemplateDictionaryInterface* dictionary,
                             bool prettyPrint);

    static bool applyAt(int dirFd, const std::string& fileName,
                        const ctemplate::TemplateDictionaryInterface* dictionary,
                        bool prettyPrint);

private:
    std::map<std::string, std::string> mExtraEnvVars;

private:
    static pthread_rwlock_t mInstanceLock;
    static DobbyTemplate* mInstance;
};


#endif // !defined(DOBBYTEMPLATE_H)
