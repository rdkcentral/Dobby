/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
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
 * File: PerfettoPlugin.h
 *
 */
#ifndef PERFETTOPLUGIN_H
#define PERFETTOPLUGIN_H

#include <IDobbyPlugin.h>
#include <PluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

// -----------------------------------------------------------------------------
/**
 * @class PerfettoPlugin
 * @brief Dobby plugin for granting access to system perfetto tracing in the
 * container.
 *
 * For now this just bind mounts in the standard perfetto socket used for IPC.
 *
 */
class PerfettoPlugin : public PluginBase
{
public:
    PerfettoPlugin(const std::shared_ptr<IDobbyEnv> &env,
                   const std::shared_ptr<IDobbyUtils> &utils);
    ~PerfettoPlugin() final;

public:
    std::string name() const final;

    unsigned hookHints() const final;

public:
    bool postConstruction(const ContainerId& id,
                          const std::shared_ptr<IDobbyStartState>& startupState,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) final;

private:
    const std::string mName;
    const std::shared_ptr<IDobbyUtils> mUtilities;

    const std::string mDefaultPerfettoSockPath;

};

#endif // !defined(PERFETTOPLUGIN_H)
