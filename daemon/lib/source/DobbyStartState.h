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
 * File:   DobbyStartState.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef DOBBYSTARTSTATE_H
#define DOBBYSTARTSTATE_H

#include <ContainerId.h>
#include <IDobbyStartState.h>

#include <list>
#include <memory>
#include <mutex>
#include <string>


class DobbyConfig;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyStartState
 *  @brief Stores the start state of the container
 *  @see IDobbyStartState
 *
 *  One of these objects is created when a container is first initialised, it
 *  stores some state and is passed to the postConstruction hook so plugins
 *  can add extra file descriptors or environment variables to the container.
 *
 *  This class is thrown away once the container is launched.
 */
class DobbyStartState : public IDobbyStartState
{
public:
    DobbyStartState(const std::shared_ptr<DobbyConfig>& config,
                    const std::list<int>& files);
    ~DobbyStartState() final;

public:
    bool isValid() const;

    const std::list<int>& files() const;

public:
    int addFileDescriptor(int fd) override;

    bool addEnvironmentVariable(const std::string& envVar) override;

    bool addMount(const std::string& source,
                  const std::string& target,
                  const std::string& fsType,
                  unsigned long mountFlags,
                  const std::list<std::string>& mountOptions) override;

private:
    const std::shared_ptr<DobbyConfig> mConfig;
    std::list<int> mFiles;
    bool mValid;
    mutable std::mutex mLock;
};


#endif // !defined(DOBBYSTARTSTATE_H)
