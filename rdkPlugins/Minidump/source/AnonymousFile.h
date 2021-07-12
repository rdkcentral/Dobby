/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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
 * File: AnonymousFile.h
 *
 */
#ifndef ANONYMOUSFILE_H
#define ANONYMOUSFILE_H

#include <string>

class AnonymousFile
{
public:
    explicit AnonymousFile(size_t size);
    explicit AnonymousFile(int fd);

public:
    int create();
    bool copyContentTo(const std::string& destFile);

private:
    long getFileSize(FILE* fp);

private:
    size_t mSize;
    int mFd;
};

#endif // !defined(ANONYMOUSFILE_H)
