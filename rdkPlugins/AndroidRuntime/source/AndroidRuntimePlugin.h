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

#ifndef ANDROIDRUNTIMEPLUGIN_H
#define ANDROIDRUNTIMEPLUGIN_H

#include <RdkPluginBase.h>
#include <complex>

/**
 * @brief Dobby AndroidRuntime plugin.
 *
 * This plugin mounts and Android system into the container:
 *      - system.img
 *      - vendor.img
 *      - data and cache directory
 *      - kernel command line
 *
 */
class AndroidRuntimePlugin : public RdkPluginBase
{
public:
    AndroidRuntimePlugin(std::shared_ptr<rt_dobby_schema> &containerConfig,
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
    bool postHalt() override;

public:
    std::vector<std::string> getDependencies() const override;

private:
    bool doMounts();
    bool doLoopMount(const std::string &src, const std::string &dest);
    bool doBindMount(const std::string &src, const std::string &dest);
    bool doBindFile(const std::string &src, const std::string &dest);
    bool doTmpfsMount(const std::string &dest);
    bool doUnmounts();
    const std::string mName;
    const std::string mRootfsPath;
    std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::shared_ptr<DobbyRdkPluginUtils> mUtils;

    bool mValid = false;

    std::string mRootFsType;

    std::string mSystemPath;
    std::string mVendorPath;
    std::string mDataPath;
    std::string mCachePath;
    std::string mCmdlinePath;
    std::string mApkPath;
    std::vector<std::string> mMounted;
};

#endif // !defined(ANDROIDRUNTIMEPLUGIN_H)
