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

#include "NullSink.h"

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <algorithm>

#include <Logging.h>

NullSink::NullSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig)
    : mContainerConfig(containerConfig),
      mContainerId(containerId),
      mBuf{}
{
    AI_LOG_FN_ENTRY();

    mDevNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);
    if (mDevNullFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to open /dev/null");
    }

    AI_LOG_FN_EXIT();
}

NullSink::~NullSink()
{
    if (mDevNullFd > 0)
    {
        if (close(mDevNullFd) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close /dev/null stream");
        }
    }
}

void NullSink::DumpLog(const int bufferFd)
{
    memset(mBuf, 0, sizeof(mBuf));

    std::lock_guard<std::mutex> locker(mLock);

    ssize_t ret;
    while (true)
    {
        ret = read(bufferFd, mBuf, sizeof(mBuf));
        if (ret <= 0)
        {
            break;
        }

        write(mDevNullFd, mBuf, ret);
    }
}

void NullSink::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, epoll_event event)
{
    std::lock_guard<std::mutex> locker(mLock);

    // Got some data, yay
    if (event.events & EPOLLIN)
    {
        ssize_t ret;
        memset(mBuf, 0, sizeof(mBuf));

        while (true)
        {
            ret = TEMP_FAILURE_RETRY(read(event.data.fd, mBuf, sizeof(mBuf)));

            if (ret < 0)
            {
                // We've reached the end of the data we can read so we're done here
                if (errno == EWOULDBLOCK)
                {
                    return;
                }

                // Something went wrong whilst reading
                AI_LOG_SYS_ERROR(errno, "Read from container %s tty failed", mContainerId.c_str());
                return;
            }

            if (write(mDevNullFd, mBuf, ret) < 0)
            {
                AI_LOG_SYS_ERROR(errno, "Write to /dev/null stream failed");
            }
        }

        return;
    }
    else if (event.events & EPOLLHUP)
    {
        pollLoop->delSource(shared_from_this(), event.data.fd);

        // Clean up - close the ptty fd
        if (close(event.data.fd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container ptty fd %d", event.data.fd);
        }
    }

    // Don't handle any other events
    return;
}