#include "FileSink.h"

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <limits.h>


#include <Logging.h>

FileSink::FileSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig)
    : mContainerConfig(containerConfig),
      mContainerId(containerId),
      mLoggingOptions({}),
      mOutputFileFd(openFile())
{
    AI_LOG_FN_ENTRY();

    mDevNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);
    if (mDevNullFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to open /dev/null");
    }

    AI_LOG_FN_EXIT();
}

FileSink::~FileSink()
{
    close(mDevNullFd);

    if (mOutputFileFd > 0)
    {
        close(mOutputFileFd);
    }

    closeFile();
}

void FileSink::SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions& options)
{
    mLoggingOptions = options;
    AI_LOG_INFO("ptty fd - %d", mLoggingOptions.pttyFd);
}

void FileSink::DumpLog(const int bufferFd, const bool startNewLog)
{
    // // Starting a new log file? Close the existing file and open a new one so we truncate
    // if (startNewLog)
    // {
    //     AI_LOG_INFO("STARTING NEW FILE!");
    //     if (mOutputFileFd > 0)
    //     {
    //         closeFile();
    //         mOutputFileFd = openFile();
    //     }
    // }
    char buf[PTY_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));

    ssize_t ret;
    ssize_t offset = 0;

    bool limitHit = false;
    while (true)
    {
        ret = read(bufferFd, buf, sizeof(buf));
        if (ret <= 0)
        {
            break;
        }

        offset += ret;

        if (offset <= mFileSizeLimit)
        {
            // Write to the output file
            if (write(mOutputFileFd, buf, ret) < 0)
            {
                AI_LOG_SYS_ERROR(errno, "Write failed");
            }
        }
        else
        {
            // Hit the limit, send the data into the void
            if (!limitHit)
            {
                AI_LOG_WARN("Logger for container %s has hit maximum size of %zu",
                            mContainerId.c_str(), mFileSizeLimit);
            }
            limitHit = true;
            write(mDevNullFd, buf, ret);
        }
    }
}

void FileSink::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
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

void FileSink::closeFile()
{
    AI_LOG_INFO("Closing output fd");
    close(mOutputFileFd);
}

int FileSink::openFile()
{
    const mode_t mode = 0644;

    int flags;
    // // Do we want to append to the file or create a new empty file?
    // if (append)
    // {
    //     flags = O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC;
    // }
    // else
    // {
    //     flags = O_CREAT | O_APPEND | O_WRONLY | O_CLOEXEC;
    // }

    flags = O_CREAT | O_TRUNC | O_APPEND | O_WRONLY | O_CLOEXEC;

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
        mFileSizeLimit = SSIZE_MAX;

    int openedFd = -1;

    // Open both our file and /dev/null (so we can send to null if we hit the limit)
    int devNullFd = open("/dev/null", O_CLOEXEC | O_WRONLY);

    if (pathName.empty())
    {
        AI_LOG_ERROR("Log settings set to log to file but no path provided. Sending to /dev/null");
        openedFd = devNullFd;
    }
    else
    {
        AI_LOG_INFO(">>> About to open %s", pathName.c_str());
        openedFd = open(pathName.c_str(), flags, mode);
        if (openedFd < 0)
        {
            // throw away everything we receive
            AI_LOG_SYS_ERROR(errno, "failed to open/create '%s'", pathName.c_str());
            openedFd = devNullFd;
        }
    }

    return openedFd;
}