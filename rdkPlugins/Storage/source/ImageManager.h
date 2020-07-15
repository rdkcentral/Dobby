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


#ifndef IMAGEMANAGER_H
#define	IMAGEMANAGER_H

#include <string>

#include <sys/types.h>

class ImageManager
{
public:
    static bool checkFSImage(const std::string & filepath,
                             uid_t userId,
                             bool fix = true);

    static bool checkFSImageAt(int dirFd,
                               const std::string & filepath,
                               uid_t userId,
                               bool fix = true);

    static bool createFSImage(const std::string & filepath,
                              uid_t userId,
                              size_t size = (12 * 1024 * 1024),
                              const std::string fs = "ext4");

    static bool createFSImageAt(int dirFd,
                                const std::string & filepath,
                                uid_t userId,
                                size_t size = (12 * 1024 * 1024),
                                const std::string fs = "ext4");

    static void deleteFSImage(const std::string & filepath);

    static void deleteFSImageAt(int dirFd,
                                const std::string & filepath);

private:
    static const std::string xAttrUserIdName;
};

#endif // defined(IMAGEMANAGER_H)