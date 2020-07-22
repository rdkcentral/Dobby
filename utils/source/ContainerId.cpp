/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
 * File:   ContainerId.cpp
 *
 */
#include "ContainerId.h"

#include <ctype.h>


// -----------------------------------------------------------------------------
/**
 *  @brief Checks if the supplied string is a valid container id
 *
 *  This simply checks that the string contains only alhpa numeric characters
 *  plus '.' and '-', in addition no double '.' is allowed.
 *
 *  In addition we must have at least one alpha character, this avoids people
 *  creating stupid names like '.' or using just numbers which could be
 *  confused with container descriptors.
 *
 *  @param[in]  id      The string to check
 *
 *  @return true if valid, otherwise false.
 */
static bool isValidContainerId(const std::string& id)
{
    if (id.empty() || (id.size() > 128))
    {
        return false;
    }

    unsigned alphaCount = 0;
    for (const char c : id)
    {
        if (!isalnum(c) && (c != '.') && (c != '-'))
            return false;

        if (isalpha(c))
            alphaCount++;
    }

    // I don't think we really need to bother with double '.' as we don't have
    // any slashes ... but that's what AICommon::AppId does, so what the hell
    if (id.find("..") != std::string::npos)
    {
        return false;
    }

    return (alphaCount > 0);
}




ContainerId ContainerId::create(const std::string& s)
{
    std::string str(s);
    ContainerId id;

    if (isValidContainerId(str))
        id.mId.swap(str);

    return id;
}

ContainerId ContainerId::create(const char* s)
{
    std::string str(s);
    ContainerId id;

    if (isValidContainerId(str))
        id.mId.swap(str);

    return id;
}

ContainerId ContainerId::create(const char* s, size_t n)
{
    std::string str(s, n);
    ContainerId id;

    if (isValidContainerId(str))
        id.mId.swap(str);

    return id;
}

