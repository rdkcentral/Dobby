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
 * DobbyFactory.h
 * Author:
 *
 */
#ifndef DOBBYFACTORY_H
#define DOBBYFACTORY_H

#include <mutex>
#include <memory>
#include <string>
#include <future>

namespace AI_IPC
{
    class IIpcService;
}

class IDobbyProxy;


// -----------------------------------------------------------------------------
/**
 *  @class DobbyFactory
 *  @brief Factory that spawns the DobbyDaemon and supplies a proxy to it.
 *
 *  The setters on the factory should be called prior to the getProxy call, this
 *  is because the paths and platform ident is passed to the daemon when it
 *  is launched.
 *
 *
 */
class DobbyFactory
{
public:
    explicit DobbyFactory(const std::shared_ptr<AI_IPC::IIpcService> &ipcService);
    ~DobbyFactory();

public:
    void setWorkspacePath(const std::string& path);
    void setFlashMountPath(const std::string& path);
    void setPlatformIdent(const std::string& platformIdent);
    void setPlatformType(const std::string& platformType);
    void setPlatformModel(const std::string& platformModel);

public:
    std::shared_ptr<IDobbyProxy> getProxy();

private:
    bool startDobbyDaemon();
    bool pingDobbyDaemon();

private:
    std::mutex mLock;
    std::string mWorkspacePath;
    std::string mFlashMountPath;
    std::string mPlatformIdent;
    std::string mPlatformType;
    std::string mPlatformModel;

private:
    std::shared_ptr<AI_IPC::IIpcService> mIpcService;
    std::shared_ptr<IDobbyProxy> mProxy;
};

#endif // !defined(DOBBYFACTORY_H)

