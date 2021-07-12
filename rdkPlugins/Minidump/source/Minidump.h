/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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
 * File: Minidump.h
 *
 */
#ifndef MINIDUMP_H
#define MINIDUMP_H

#include <RdkPluginBase.h>

#include <string>
#include <memory>

/**
 * @brief Dobby RDK Minidump Plugin
 *
 * Manages collection of minidump core files located in container namespace
 */
class Minidump : public RdkPluginBase
{
public:
    Minidump(std::shared_ptr<rt_dobby_schema>& containerConfig,
             const std::shared_ptr<DobbyRdkPluginUtils> &utils,
             const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    unsigned hookHints() const override;

public:
    // This hook creates anonymous file and adds it into preserve container list
    bool preCreation() override;

    // This hook copies minidump file to host namespace
    bool postHalt() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    std::string getDestinationFile();

    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};

#endif // !defined(MINIDUMP_H)
