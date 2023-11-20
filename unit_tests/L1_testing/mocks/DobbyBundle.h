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

#ifndef DOBBYBUNDLE_H
#define DOBBYBUNDLE_H

#include "IDobbyUtils.h"
#include "ContainerId.h"

class IDobbyEnv;

class DobbyBundleImpl {

public:

    virtual void setPersistence(bool persist) = 0;
    virtual bool isValid() const = 0;
    virtual const std::string& path() const = 0;

};

class DobbyBundle {

protected:
    static DobbyBundleImpl* impl;

public:

    DobbyBundle(){}

#if defined(LEGACY_COMPONENTS)

    DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,const std::shared_ptr<const IDobbyEnv>& env, const ContainerId& id){}
#endif // defined(LEGACY_COMPONENTS)

    DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,const std::shared_ptr<const IDobbyEnv>& env,const std::string& bundlePath){}
    ~DobbyBundle(){}

    const std::string& path() const;


    static void setImpl(DobbyBundleImpl* newImpl);
    static DobbyBundle* getInstance();
    static void setPersistence(bool persist);
    static bool isValid();
};

#endif // !defined(DOBBYBUNDLE_H)

