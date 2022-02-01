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

#include "FileSink.h"

#include <Logging.h>

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>

/**
 * @brief A logging sink that sends the contents of the container stdout/err to a given file. The file
 * can have a size limit set.
 *
 * Will create the requested file providing the directory exists. Creates a new file each time
 * this class is instantiated
 *
 */
FileSink::FileSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig)
    : mContainerConfig(containerConfig),
      mContainerId(containerId),
      mLoggingOptions{},
      mLimitHit(false),
      mBuf{}
{
    AI_LOG_FN_ENTRY();

    // If we can't open dev/null something weird is going on
    mDevNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);
    if (mDevNullFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to open /dev/null");
    }

    if (mContainerConfig->rdk_plugins->logging->data->file_options)
    {
        mOutputFilePath = mContainerConfig->rdk_plugins->logging->data->file_options->path;
        if (mContainerConfig->rdk_plugins->logging->data->file_options->limit_present)
        {
            mFileSizeLimit = mContainerConfig->rdk_plugins->logging->data->file_options->limit;
            // Set to -1 in config for "unlimited"
            if (mFileSizeLimit < 0)
            {
                mFileSizeLimit = SSIZE_MAX;
            }
        }
        else
        {
            AI_LOG_INFO("No file size limit size for container log - setting to unlimited");
            mFileSizeLimit = SSIZE_MAX;
        }
    }

    mOutputFileFd = openFile(mOutputFilePath);
    if (mOutputFileFd < 0)
    {
        // Couldn't open our output file, send to /dev/null to avoid blocking
        AI_LOG_SYS_ERROR(errno, "Failed to open container logfile - sending to /dev/null");
        if (mDevNullFd > 0)
        {
            mOutputFileFd = mDevNullFd;
        }
    }

    AI_LOG_FN_EXIT();
}

FileSink::~FileSink()
{
    // Close everything we opened
    if (close(mDevNullFd) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to close /dev/null");
    }

    if (mOutputFileFd > 0)
    {
        if (close(mOutputFileFd) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close output file");
        }
    }
}

/**
 * @brief Sets the log options used by the process() method
 */
void FileSink::SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions &options)
{
    std::lock_guard<std::mutex> locker(mLock);
    mLoggingOptions = options;
}

/**
 * @brief Reads all the available data from the provided fd and writes it to the output
 * file. Does not attempt to seek the file descriptor back to the start
 *
 * If file limit is hit, will send data to /dev/null
 *
 * @param[in]   bufferFd    The fd to read from
 */
void FileSink::DumpLog(const int bufferFd)
{
    std::lock_guard<std::mutex> locker(mLock);

    memset(mBuf, 0, sizeof(mBuf));

    ssize_t ret;
    ssize_t offset = 0;

    // Read all the data from the provided file descriptor
    while (true)
    {
        ret = read(bufferFd, mBuf, sizeof(mBuf));
        if (ret <= 0)
        {
            break;
        }

        offset += ret;

        if (offset <= mFileSizeLimit)
        {
            // Write to the output file
            if (write(mOutputFileFd, mBuf, ret) < 0)
            {
                AI_LOG_SYS_ERROR(errno, "Write failed");
            }
        }
        else
        {
            // Hit the limit, send the data into the void
            if (!mLimitHit)
            {
                AI_LOG_WARN("Logger for container %s has hit maximum size of %zu",
                            mContainerId.c_str(), mFileSizeLimit);
            }
            mLimitHit = true;
            write(mDevNullFd, mBuf, ret);
        }
    }
}

/**
 * @brief Called by the pollLoop when an event occurs on the container ptty.
 *
 * Reads the contents of the ptty and logs to a file
 */
void FileSink::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
    std::lock_guard<std::mutex> locker(mLock);

    if (events & EPOLLIN)
    {
        ssize_t ret;
        ssize_t offset = 0;

        memset(mBuf, 0, sizeof(mBuf));

        while (true)
        {
            ret = TEMP_FAILURE_RETRY(read(mLoggingOptions.pttyFd, mBuf, sizeof(mBuf)));
            if (ret <= 0)
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

            offset += ret;
            if (offset <= mFileSizeLimit)
            {
                // Write to the output file
                if (write(mOutputFileFd, mBuf, ret) < 0)
                {
                    AI_LOG_SYS_ERROR(errno, "Write to %s failed", mOutputFilePath.c_str());
                }
            }
            else
            {
                // Hit the limit, send the data into the void
                if (!mLimitHit)
                {
                    AI_LOG_WARN("Logger for container %s has hit maximum size of %zu",
                                mContainerId.c_str(), mFileSizeLimit);
                }
                mLimitHit = true;
                write(mDevNullFd, mBuf, ret);
            }
        }

        return;
    }

    if (events & EPOLLHUP)
    {
        AI_LOG_DEBUG("EPOLLHUP! Removing ourself from the event loop!");

        // Remove ourselves from the event loop
        pollLoop->delSource(shared_from_this());

        // Clean up
        if (close(mLoggingOptions.pttyFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container ptty fd %d", mLoggingOptions.pttyFd);
        }

        if (close(mLoggingOptions.connectionFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container connection %d", mLoggingOptions.connectionFd);
        }

        return;
    }
}

/**
 * @brief Opens the log file at a given path. Will create a new file
 * when called, and subsequent writes will append to the file
 *
 * @param[in]   pathName    Where to create the file
 *
 * @return Opened file descriptor
 */
int FileSink::openFile(const std::string& pathName)
{
    const mode_t mode = 0644;
    int flags = O_CREAT | O_TRUNC | O_APPEND | O_WRONLY | O_CLOEXEC;

    int openedFd = -1;
    if (pathName.empty())
    {
        AI_LOG_ERROR("Log settings set to log to file but no path provided");
        return -1;
    }
    else
    {
        openedFd = open(pathName.c_str(), flags, mode);
        if (openedFd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to open/create '%s'", pathName.c_str());
            return -1;
        }
    }

    return openedFd;
}