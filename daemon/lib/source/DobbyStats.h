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
 * File:   DobbyStats.h
 *
 * Copyright (C) BSKYB 2016+
 */
#ifndef DOBBYSTATS_H
#define DOBBYSTATS_H

#include <ContainerId.h>
#include <IDobbyEnv.h>

#include <memory>
#include <string>

#if defined(RDK)
#  include <json/json.h>
#else
#  include <jsoncpp/json.h>
#endif

class IDobbyEnv;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyStats
 *  @brief Small utility class used to get the stats of a container
 *
 *  There is not much to this class, it's just a collection of utility functions
 *  for gathering statistics on a container with a given id.
 *
 *  Note that the code in this class tends not to print errors if it can't
 *  gather some stats, that's by design as it's meant to be a best effort.
 *
 */
class DobbyStats
{
public:
    DobbyStats(const ContainerId& id,
               const std::shared_ptr<IDobbyEnv>& env);
    ~DobbyStats();

public:
    const Json::Value& stats() const;

private:
    static ssize_t readCgroupFile(const ContainerId& id,
                                  const std::string& cgroupMntPath,
                                  const std::string& cgroupfileName,
                                  char* buf, size_t bufLen);

    static Json::Value readSingleCgroupValue(const ContainerId& id,
                                             const std::string& cgroupMntPath,
                                             const std::string& cgroupfileName);

    static  Json::Value readMultipleCgroupValues(const ContainerId& id,
                                                 const std::string& cgroupMntPath,
                                                 const std::string& cgroupfileName);

    static Json::Value getStats(const ContainerId& id,
                                const std::shared_ptr<IDobbyEnv>& env);

private:
    const Json::Value mStats;
};


#endif // !defined(DOBBYSTATS_H)
