/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2022 Sky UK
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
 * File: MountProperties.h
 *
 */
#ifndef MOUNTPROPERTIES_H
#define MOUNTPROPERTIES_H

#include <sys/types.h>
#include <string>
#include <list>


/**
*  @brief LoopMountProperties struct used for Storage plugin
*/
typedef struct _LoopMountProperties
{
    std::string fsImagePath;
    std::string fsImageType;
    std::string destination;
    std::list<std::string> mountOptions;
    unsigned long mountFlags;
    bool persistent;
    int imgSize;
    bool imgManagement;

} LoopMountProperties;

/**
*  @brief DynamicMountProperties struct used for Storage plugin
*/
typedef struct _DynamicMountProperties
{
    std::string source;
    std::string destination;
    std::list<std::string> mountOptions;
    unsigned long mountFlags;

} DynamicMountProperties;

/**
*  @brief MountOwnerProperties struct used for Storage plugin
*/
typedef struct _MountOwnerProperties
{
    std::string source;
    std::string user;
    std::string group;
    bool recursive;

} MountOwnerProperties;

#endif // !defined(MOUNTPROPERTIES_H)
