/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2023 Synamedia
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

#ifndef DOBBYROOTFS_H
#define DOBBYROOTFS_H

#include "IDobbyUtils.h"
#include "ContainerId.h"

#if defined(LEGACY_COMPONENTS)
#include "DobbySpecConfig.h"
#endif // LEGACY_COMPONENTS

#include "DobbyBundleConfig.h"

class DobbyBundle;

class DobbyRootfsImpl {
public:

    virtual void setPersistence(bool persist) = 0;

    virtual const std::string& path() const = 0;

    virtual bool isValid() const = 0;

};

class DobbyRootfs {

protected:
    static DobbyRootfsImpl* impl;

public:

    DobbyRootfs();
#if defined(LEGACY_COMPONENTS)
    DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const DobbyBundle>& bundle,const std::shared_ptr<const DobbySpecConfig>& config);
#endif // LEGACY_COMPONENTS

    DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,const std::shared_ptr<const DobbyBundle>& bundle,const std::shared_ptr<const DobbyBundleConfig>& config);
    ~DobbyRootfs();

    static void setImpl(DobbyRootfsImpl* newImpl);
    static DobbyRootfs* getInstance();
    static void setPersistence(bool persist);
    static const std::string& path();
    static bool isValid();
};

#endif // !defined(DOBBYROOTFS_H)


