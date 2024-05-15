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

#ifndef DOBBYSTARTSTATE_H
#define DOBBYSTARTSTATE_H

#include <set>
#include <bitset>
#include <memory>

#include <ContainerId.h>
#include "IDobbyStartState.h"

class DobbyConfig;

class DobbyStartStateImpl {
public:

    virtual ~DobbyStartStateImpl() = default;
    virtual std::list<int> files() const =0;
    virtual bool isValid() const = 0;
};

class DobbyStartState : public IDobbyStartState{

protected:
    static DobbyStartStateImpl* impl;

public:

    DobbyStartState();
    DobbyStartState(const std::shared_ptr<DobbyConfig>& config,const std::list<int>& files);
    ~DobbyStartState();
    static void setImpl(DobbyStartStateImpl* newImpl);
    bool isValid() const;
    std::list<int> files() const;

};


#endif // !defined(DOBBYSTARTSTATE_H)

