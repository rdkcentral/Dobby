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
#include "StdStreamPipe.h"

#include <Logging.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

/**
 * @brief Creates a new pipe that can be used to capture std streams.
 *
 * @param logPipeContents If true, then log the contents of the pipe when
 * destructed (useful for capturing stderr)
 */
StdStreamPipe::StdStreamPipe(bool logPipeContents)
    : mReadFd(-1),
      mWriteFd(-1),
      mLogPipe(logPipeContents)
{
    int fds[2];
    if (pipe2(fds, O_CLOEXEC | O_NONBLOCK) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create pipe");
    }
    else
    {
        mReadFd = fds[0];
        mWriteFd = fds[1];
    }
}

StdStreamPipe::~StdStreamPipe()
{
    if ((mWriteFd >= 0) && (close(mWriteFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close write pipe");

    if (mReadFd >= 0)
    {
        if (mLogPipe)
    {
            AI_LOG_ERROR("%s", getPipeContents().c_str());
        }

        if (close(mReadFd) != 0)
            AI_LOG_SYS_ERROR(errno, "failed to close read pipe");
    }
}

int StdStreamPipe::writeFd() const
{
    return mWriteFd;
}

/**
 * @brief Gets the contents of the pipe as a string
 *
 * @warning This is not thread-safe as will seek the pipe.
 */
std::string StdStreamPipe::getPipeContents() const
{
    std::string contents;
    char buf[256] = {0};

    while (true)
    {
        ssize_t ret = TEMP_FAILURE_RETRY(read(mReadFd, buf, sizeof(buf) - 1));
        // Something went wrong
        if (ret < 0)
        {
            // non-blocking pipe, so EAGAIN just means we've hit the end
            if (errno != EAGAIN)
            {
                AI_LOG_SYS_ERROR(errno, "failed to read from pipe");
            }
            break;
        }
        // Reached EOF
        if (ret == 0)
        {
            break;
        }
        contents.append(buf, sizeof(buf));
    }
    return contents;
}