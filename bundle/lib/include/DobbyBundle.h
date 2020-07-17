/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 RDK Management
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
 * File:   DobbyBundle.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef DOBBYBUNDLE_H
#define DOBBYBUNDLE_H

#include "IDobbyUtils.h"
#include "ContainerId.h"

#include <memory>
#include <string>


class IDobbyEnv;


// -----------------------------------------------------------------------------
/**
 *  @class DobbyBundle
 *  @brief Simple class that just creates a subdir in the bundles directory
 *
 *  When this object is destroyed it will delete the entire contents of the
 *  directory it created as well as the directory itself.
 *
 */
class DobbyBundle
{
public:
    DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,
                const std::shared_ptr<const IDobbyEnv>& env,
                const ContainerId& id);
    DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,
                const std::string& path,
                bool persist = true);
    DobbyBundle(const std::shared_ptr<const IDobbyUtils>& utils,
                const std::shared_ptr<const IDobbyEnv>& env,
                const std::string& bundlePath);
    ~DobbyBundle();

public:
    bool isValid() const;

    const std::string& path() const;
    int dirFd() const;

    void setPersistence(bool persist);
    const bool getPersistence() const;

private:
    const std::shared_ptr<const IDobbyUtils> mUtilities;
    bool mPersist;

private:
    std::string mPath;
    int mDirFd;
};


#endif // !defined(DOBBYBUNDLE_H)
