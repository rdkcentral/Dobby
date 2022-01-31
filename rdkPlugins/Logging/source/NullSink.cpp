#include "NullSink.h"

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <Logging.h>

NullSink::NullSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig)
    : mContainerConfig(containerConfig),
      mContainerId(containerId),
      mBuf{}
{
    // Create a file descriptor we can write to
    // journald will handle line breaks etc automatically
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
            AI_LOG_SYS_ERROR(errno, "Failed to close journald stream");
        }
    }
}

void NullSink::SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions &options)
{
    mLoggingOptions = options;
}

void NullSink::DumpLog(const int bufferFd)
{
    std::lock_guard<std::mutex> locker(mLock);

    ssize_t ret;
    memset(mBuf, 0, sizeof(mBuf));

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

void NullSink::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
    std::lock_guard<std::mutex> locker(mLock);

    // Got some data, yay
    if (events & EPOLLIN)
    {
        ssize_t ret;
        memset(mBuf, 0, sizeof(mBuf));

        ret = TEMP_FAILURE_RETRY(read(mLoggingOptions.pttyFd, mBuf, sizeof(mBuf)));

        if (ret < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Read from container tty failed");
        }

        if (write(mDevNullFd, mBuf, ret) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Write to journald stream failed");
        }

        return;
    }

    // Container shutdown
    if (events & EPOLLHUP)
    {
        AI_LOG_INFO("EPOLLHUP! Removing ourselves from the event loop!");

        // Remove ourselves from the event loop
        pollLoop->delSource(shared_from_this());

        // Clean up
        if (close(mLoggingOptions.pttyFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container ptty fd");
        }

        if (close(mLoggingOptions.connectionFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container connection");
        }

        return;
    }

    // Don't handle any other events
    return;
}