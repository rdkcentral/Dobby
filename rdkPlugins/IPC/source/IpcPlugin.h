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
 * File: IpcPlugin.h
 *
 */
#ifndef IPCPLUGIN_H
#define IPCPLUGIN_H

#include <RdkPluginBase.h>

#include <sys/types.h>
#include <netinet/in.h>

#include <unistd.h>
#include <string>
#include <memory>

/**
 * @brief IPC Plugin
 *
 * Gives access to dbus inside container
 *
 */
class IpcPlugin : public RdkPluginBase
{
public:
    IpcPlugin(std::shared_ptr<rt_dobby_schema>& containerConfig,
                  const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                  const std::string& rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    // Override to return the appropriate hints for what we implement
    unsigned hookHints() const override;

public:
    bool postInstallation() override;

private:
    bool addSocketAndEnv(const std::shared_ptr<DobbyRdkPluginUtils> utils,
                        const std::string& rootfsPath,
                        std::shared_ptr<rt_dobby_schema> containerConfig,
                        std::string busStr,
                        const std::string &socketPath,
                        const std::string &envVar) const;

    std::string socketPathFromAddressSimple(const std::string& address) const;

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;

    const std::string mDbusRunDir;

    const std::string mDbusSystemSocketPath;
    const std::string mDbusSessionSocketPath;
    const std::string mDbusDebugSocketPath;

    const std::string mDbusSystemEnvVar;
    const std::string mDbusSessionEnvVar;
    const std::string mDbusDebugEnvVar;


};

#endif // !defined(IPCPLUGIN_H)
