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
#ifndef LOGGINGPLUGIN_H
#define LOGGINGPLUGIN_H

#include <DobbyLoggerBase.h>

#include <sys/types.h>

#include <unistd.h>
#include <string>
#include <memory>
#include <mutex>

/**
 * @brief Dobby Logging plugin
 */
class LoggingPlugin : public DobbyLoggerBase
{
public:
    LoggingPlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
                  const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                  const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    bool postInstallation() override;

public:
    // Logging Specific Methods
    void LoggingLoop(IDobbyRdkLoggingPlugin::ContainerInfo containerInfo,
                     const bool isBuffer,
                     const bool createNew) override;

private:
    // Locations the plugin can send the logs
    enum class LoggingSink
    {
        DevNull,
        File,
        Journald
    };

private:
    LoggingSink GetContainerSink();
    void FileSink(const ContainerInfo &containerInfo, bool exitEof, bool createNew);
    void DevNullSink(const ContainerInfo &containerInfo, bool exitEof);
    void JournaldSink(const ContainerInfo &containerInfo, bool exitEof);

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(LOGGINGPLUGIN_H)
