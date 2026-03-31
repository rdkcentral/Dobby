/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2024 Sky UK
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

// Minimal stubs for DobbyTemplate static methods used by DobbySpecConfig.
//
// DobbySpecConfig::parseSpec calls DobbyTemplate::applyAt at step 8 to write
// the rendered config.json.  In unit tests we pass dirFd=-1 so we only need
// this to return false (which is correct behaviour for an invalid fd) – the
// preceding steps have already populated the ctemplate dictionary, which is
// what the tests actually verify.

#include "DobbyTemplate.h"

// Required by DobbyTemplateMock.cpp's delegate pattern
DobbyTemplateImpl* DobbyTemplate::impl = nullptr;

bool DobbyTemplate::applyAt(int /* dirFd */,
                            const std::string& /* fileName */,
                            const ctemplate::TemplateDictionaryInterface* /* dict */,
                            bool /* prettyPrint */)
{
    // Stub: always returns false so parseSpec returns false, but the
    // dictionary is fully populated by the time this is called.
    return false;
}

std::string DobbyTemplate::apply(const ctemplate::TemplateDictionaryInterface* /* dict */,
                                 bool /* prettyPrint */)
{
    return "";
}
