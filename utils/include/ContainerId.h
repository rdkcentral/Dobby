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
 * File:   ContainerId.h
 *
 * Copyright (C) Sky UK 2016+
 */
#ifndef CONTAINERID_H
#define CONTAINERID_H

#include <cstdint>
#include <string>

// -----------------------------------------------------------------------------
/**
 *  @class ContainerId
 *  @brief A wrapper around a std::string, used to add some type definition to
 *  to an id and also to sanity check the characters that make up a container
 *  id.
 *
 *  This class is modelled on the AICommon::AppId, however wanted to distinguish
 *  because this daemon can be used to launch non-app processes.
 *
 */
class ContainerId
{
public:
    static ContainerId create(const std::string& s);
    static ContainerId create(const char* s);
    static ContainerId create(const char* s, size_t n);

public:
    ContainerId() = default;
    ContainerId(ContainerId&&) = default;
    ContainerId(const ContainerId&) = default;
    ContainerId& operator=(const ContainerId&) = default;
    ContainerId& operator=(ContainerId&&) = default;

public:
    ~ContainerId() = default;

    bool isValid() const
    {
        return !mId.empty();
    }

    const std::string& str() const
    {
        return mId;
    }

    const char* c_str() const
    {
        return mId.c_str();
    }

public:
    bool operator==(const ContainerId& rhs) const
    {
        return mId == rhs.mId;
    }

    bool operator!=(const ContainerId& rhs) const
    {
        return mId != rhs.mId;
    }

    bool operator<(const ContainerId& rhs) const
    {
        return mId < rhs.mId;
    }

    bool operator>(const ContainerId& rhs) const
    {
        return mId > rhs.mId;
    }

private:
    std::string mId;
};


#endif // !defined(CONTAINERID_H)
