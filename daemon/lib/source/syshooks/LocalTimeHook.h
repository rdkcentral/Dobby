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
 * File:   LocalTimeHook.h
 *
 */
#ifndef LOCALTIMEHOOK_H
#define LOCALTIMEHOOK_H

#include "BaseHook.h"

#include <sys/types.h>

#include <map>
#include <string>
#include <thread>
#include <memory>


// -----------------------------------------------------------------------------
/**
 *  @class LocalTimeHook
 *  @brief Creates the /etc/localtime symlink inside the container to match
 *  the system.
 *
 *
 *
 */
class LocalTimeHook : public BaseHook
{
public:
    explicit LocalTimeHook(const std::shared_ptr<IDobbyUtils>& utils);
    ~LocalTimeHook() final;

public:
    std::string hookName() const override;

    unsigned hookHints() const override;

    bool postConstruction(const ContainerId& id,
                          const std::shared_ptr<IDobbyStartState>& startupState,
                          const std::shared_ptr<const DobbyConfig>& config,
                          const std::shared_ptr<const DobbyRootfs>& rootfs) override;

private:
    const std::shared_ptr<IDobbyUtils> mUtilities;

private:
    std::string mTimeZonePath;
};


#endif // !defined(LOCALTIMEHOOK_H)
