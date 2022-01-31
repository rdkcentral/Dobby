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

    mOutputFileFd = openFile();
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

void FileSink::SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions& options)
{
    mLoggingOptions = options;
}

void FileSink::DumpLog(const int bufferFd)
{
    // Take the lock
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

void FileSink::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
    std::lock_guard<std::mutex> locker(mLock);

    if (events & EPOLLIN)
    {
        ssize_t ret;
        ssize_t offset = 0;

        memset(mBuf, 0, sizeof(mBuf));

        ret = read(mLoggingOptions.pttyFd, mBuf, sizeof(mBuf));
        if (ret <= 0)
        {
            return;
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

        return;
    }

    if (events & EPOLLHUP)
    {
        AI_LOG_INFO("EPOLLHUP! Removing ourself from the event loop!");

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

int FileSink::openFile()
{
    const mode_t mode = 0644;
    int flags = O_CREAT | O_TRUNC | O_APPEND | O_WRONLY | O_CLOEXEC;

    // Read the options from the config if possible
    std::string pathName;
    if (mContainerConfig->rdk_plugins->logging->data->file_options)
    {
        pathName = mContainerConfig->rdk_plugins->logging->data->file_options->path;
        if (mContainerConfig->rdk_plugins->logging->data->file_options->limit_present)
        {
            mFileSizeLimit = mContainerConfig->rdk_plugins->logging->data->file_options->limit;
        }
    }

    // if limit is -1 it means unlimited, but to make life easier just set it
    // to the max value
    if (mFileSizeLimit < 0)
    {
        mFileSizeLimit = SSIZE_MAX;
    }

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