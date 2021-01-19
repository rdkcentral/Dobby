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
 * File: OpenCDMPlugin.h
 *
 */
#ifndef OPENCDMPLUGIN_H
#define OPENCDMPLUGIN_H

#include <IDobbyPlugin.h>
#include <PluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

// -----------------------------------------------------------------------------
/**
 * @class OpenCDMPlugin
 * @brief Dobby plugin for creating the necessary OCDM buffers
 * 
 * To launch the WPE runtime, various OpenCDM temporary files and sockets are
 * needed inside the /tmp directory to allow decryption of DRM content to take
 * place using the OCDMi plugin.
 * 
 * RunC cannot mount files that don't exist, so we need to create the files
 * before we can launch the container
 * 
 */
class OpenCDMPlugin : public PluginBase
{
public:
    OpenCDMPlugin(const std::shared_ptr<IDobbyEnv> &env,
                  const std::shared_ptr<IDobbyUtils> &utils);
    ~OpenCDMPlugin() override;

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
    const gid_t mAppsGroupId;

private:
    std::string ocdmBufferPath(unsigned bufferNum) const;
    std::string ocdmBufferAdminPath(unsigned bufferNum) const;
    bool writeFileIfNotExists(const std::string &filePath) const;
    bool enableTmpOCDMDir(const std::shared_ptr<IDobbyStartState>& startupState) const;
};

#endif // !defined(OPENCDMPLUGIN_H)
