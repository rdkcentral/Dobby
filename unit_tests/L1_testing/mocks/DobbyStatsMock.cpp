/* If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2023 Synamedia
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
*/

#include "DobbyStatsMock.h"

DobbyStats::DobbyStats()
{
}

DobbyStats::DobbyStats(const ContainerId &id,const std::shared_ptr<IDobbyEnv> &env,const std::shared_ptr<IDobbyUtils> &utils)
{
}

DobbyStats::~DobbyStats()
{
}

void DobbyStats::setImpl(DobbyStatsImpl* newImpl)
{
    impl = newImpl;
}

DobbyStats* DobbyStats::getInstance()
{
    static DobbyStats* instance = nullptr;
    if (nullptr == instance)
    {
       instance = new DobbyStats();
    }
    return instance;
}

const Json::Value & DobbyStats::stats()
{
   EXPECT_NE(impl, nullptr);

    return impl->stats();
}
