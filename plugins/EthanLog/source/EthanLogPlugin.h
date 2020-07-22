/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
 * File:   EthanLogPlugin.h
 *
 */
#ifndef ETHANLOGPLUGIN_H
#define ETHANLOGPLUGIN_H

#include <IDobbyPlugin.h>
#include <PluginBase.h>

#include <string>
#include <mutex>


class EthanLogLoop;

// -----------------------------------------------------------------------------
/**
 *  @class EthanLogPlugin
 *  @brief Plugin library that create a DIAG logging pipe for a container
 *
 *  This plugin uses the IPC / DBUS fusion interface to request a new logging
 *  pipe from APP_Process (the AI AppLogger component).  The write end of the
 *  pipe file descriptor is then inserted into the container and an environment
 *  variable is set with the number of the fd.
 *
 *
 *
 */
class EthanLogPlugin : public PluginBase
{
public:
    EthanLogPlugin(const std::shared_ptr<IDobbyEnv>& env,
                   const std::shared_ptr<IDobbyUtils>& utils);
    ~EthanLogPlugin() final;

public:
    std::string name() const final;

    unsigned hookHints() const final;

public:
    bool postConstruction(const ContainerId& id,
                          const std::shared_ptr<IDobbyStartState>& startupState,
                          const std::string& rootfsPath,
                          const Json::Value& jsonData) final;

    bool preStart(const ContainerId& id,
                  pid_t pid,
                  const std::string& rootfsPath,
                  const Json::Value& jsonData) final;

private:
    unsigned parseLogLevels(const Json::Value& jsonArray) const;

private:
    const std::string mName;
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::shared_ptr<EthanLogLoop> mLogLoop;

private:
    const unsigned mDefaultLogLevelsMask;
    int mDevNullFd;
};


#endif // !defined(ETHANLOGPLUGIN_H)
