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
/*
 * File:   DobbyRunc.h
 *
 */

#ifndef DOBBYSTATS_H
#define DOBBYSTATS_H

#include <ContainerId.h>
#include <IDobbyEnv.h>
#include "IDobbyUtils.h"

#include <memory>
#include <string>

#if defined(RDK)
#include <json/json.h>
#else
#include <jsoncpp/json.h>
#endif

class IDobbyEnv;

class DobbyStatsImpl {
public:

    virtual ~DobbyStatsImpl() = default;

    virtual const Json::Value &stats() const = 0;

};

class DobbyStats {
protected:
    static DobbyStatsImpl* impl;

public:
    DobbyStats();
    DobbyStats(const ContainerId &id,const std::shared_ptr<IDobbyEnv> &env,const std::shared_ptr<IDobbyUtils> &utils);
    ~DobbyStats();

    static void setImpl(DobbyStatsImpl* newImpl);
    static DobbyStats* getInstance();
    static const Json::Value & stats();
};

#endif // !defined(DOBBYSTATS_H)


