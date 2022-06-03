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
 * File: OOMCrashPlugin.h
 *
 */
#ifndef OOMCRASH_H
#define OOMCRASH_H

#include <RdkPluginBase.h>

#include <sys/stat.h>

/**
 * @brief Dobby RDK OOMCrash Plugin
 *
 * Creates a file when OOM detected
 */
class OOMCrash : public RdkPluginBase
{
public:
    OOMCrash(std::shared_ptr<rt_dobby_schema>& containerConfig,
                  const std::shared_ptr<DobbyRdkPluginUtils> &utils,
                  const std::string &rootfsPath);

public:
    inline std::string name() const override
    {
        return mName;
    };

    // Override to return the appropriate hints for what we implement
    unsigned hookHints() const override;

public:
    bool postInstallation() override;
    bool postHalt() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    bool readCgroup(unsigned long *val);
    bool checkForOOM();
    void createFileForOOM();
    
    const std::string mName;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mRootfsPath;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;
};


#endif // !defined(OOMCRASH_H)
