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

// Link-time stubs for symbols referenced by DobbySpecConfig.cpp that are
// never called during unit testing.  The 4-arg DobbySpecConfig constructor
// does not invoke convertToCompliant or rt_dobby_schema_parse_file, and
// addGpuDevNodes / addVpuDevNodes are only reached when gpuAccessSettings()
// returns non-null (our mock returns nullptr).

#include "IpcCommon.h"      // IAsyncReplySender / IAsyncReplySenderApiImpl
#include "ContainerId.h"
#include <list>
#include <string>
#include <memory>

// ── IPC ───────────────────────────────────────────────────────────────────────
// Required by the IpcCommon.h PIMPL inline functions that are instantiated
// during test compilation even though sendReply is never called.
AI_IPC::IAsyncReplySenderApiImpl* AI_IPC::IAsyncReplySender::impl = nullptr;

// ── DobbyConfig ───────────────────────────────────────────────────────────────
// Weak stubs – never called because (a) we use the 4-arg constructor which
// skips convertToCompliant, and (b) gpuAccessSettings() returns nullptr so
// addGpuDevNodes / addVpuDevNodes are never entered.

#include "DobbyConfig.h"

bool DobbyConfig::convertToCompliant(
        const ContainerId& /*id*/,
        std::shared_ptr<rt_dobby_schema> /*cfg*/,
        const std::string& /*rootfsPath*/)
{
    return true;    // never called in these tests
}

std::list<DobbyConfig::DevNode> DobbyConfig::scanDevNodes(
        const std::list<std::string>& /*devNodes*/)
{
    return {};      // never called in these tests
}
