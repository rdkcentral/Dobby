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

#include "DobbyStartStateMock.h"

IDobbyStartState::IDobbyStartState()
{
}

IDobbyStartState::~IDobbyStartState()
{
}

DobbyStartState::DobbyStartState()
{
}

DobbyStartState::DobbyStartState(const std::shared_ptr<DobbyConfig>& config,const std::list<int>& files)
{
}

DobbyStartState::~DobbyStartState()
{
}

void DobbyStartState::setImpl(DobbyStartStateImpl* newImpl)
{
    impl = newImpl;
}

DobbyStartState* DobbyStartState::getInstance()
{
    static DobbyStartState* instance = nullptr;
    if (nullptr == instance)
    {
       instance = new DobbyStartState();
    }
    return instance;
}

std::list<int> DobbyStartState::files() const
{
   EXPECT_NE(impl, nullptr);

    return impl->files();
}

bool DobbyStartState::isValid()
{
   EXPECT_NE(impl, nullptr);

    return impl->isValid();
}

