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

#ifndef STDSTREAMPIPE_H
#define STDSTREAMPIPE_H

#include <string>

// -----------------------------------------------------------------------------
/**
 *  @class StdStreamPipe
 *  @brief Utility object that creates a pipe that can be used to capture stdout/
 *  stderr.
 *
 */
class StdStreamPipe
{
public:
    StdStreamPipe(bool logPipeContents);
    ~StdStreamPipe();

    int writeFd() const;

    std::string getPipeContents() const;

private:
    int mReadFd;
    int mWriteFd;
    bool mLogPipe;
};

#endif // !STDSTREAMPIPE_H