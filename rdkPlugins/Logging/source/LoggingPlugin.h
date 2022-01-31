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

#include "IPollLoop.h"
#include "PollLoop.h"
#include "ILoggingSink.h"

#include <sys/types.h>

#include <unistd.h>
#include <string>
#include <memory>
#include <mutex>

// Max pty buffer size is 4096
#define PTY_BUFFER_SIZE 4096

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
    std::vector<std::string> getDependencies() const override;

public:
    void RegisterPollSources(LoggingOptions &loggingOptions,
                             std::shared_ptr<AICommon::IPollLoop> pollLoop) override;

    void DumpToLog(const int bufferFd) override;

private:
    // Locations the plugin can send the logs
    enum class LoggingSink
    {
        DevNull,
        File,
        Journald
    };

private:
    std::shared_ptr<ILoggingSink> CreateSink(LoggingSink sinkType);
    LoggingSink GetContainerSink();

private:
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;

    std::shared_ptr<ILoggingSink> mSink;
    std::shared_ptr<AICommon::IPollLoop> mPollLoop;
};

#endif // !defined(LOGGINGPLUGIN_H)
