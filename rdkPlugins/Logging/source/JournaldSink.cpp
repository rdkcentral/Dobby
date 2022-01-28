#include "JournaldSink.h"

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#if defined(USE_SYSTEMD)
#include <systemd/sd-journal.h>
#endif

#include <Logging.h>

JournaldSink::JournaldSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig)
    : mContainerConfig(containerConfig),
      mContainerId(containerId)
{
    // Create a file descriptor we can write to
    // journald will handle line breaks etc automatically
    AI_LOG_FN_ENTRY();

    int logPriority = LOG_INFO;
    if (mContainerConfig->rdk_plugins->logging->data->journald_options)
    {
        std::string priority = mContainerConfig->rdk_plugins->logging->data->journald_options->priority;
        if (!priority.empty())
        {
            const std::map<std::string, int> options =
                {
                    {"LOG_EMERG", 0},
                    {"LOG_ALERT", 1},
                    {"LOG_CRIT", 2},
                    {"LOG_ERR", 3},
                    {"LOG_WARNING", 4},
                    {"LOG_NOTICE", 5},
                    {"LOG_INFO", 6},
                    {"LOG_DEBUG", 7}};

            auto it = options.find(priority);
            if (it != options.end())
            {
                logPriority = it->second;
            }
            else
            {
                AI_LOG_WARN("Could not parse journald priority - using LOG_INFO");
            }
        }
    }

    mJournaldSteamFd = sd_journal_stream_fd(mContainerId.c_str(), logPriority, 1);

    if (mJournaldSteamFd < 0)
    {
        AI_LOG_SYS_ERROR(-mJournaldSteamFd, "Failed to create journald stream fd");

        // Just use /dev/null instead
        mJournaldSteamFd = open("/dev/null", O_CLOEXEC | O_WRONLY);
    }

    AI_LOG_FN_EXIT();
}

void JournaldSink::DumpLog(const IDobbyRdkLoggingPlugin::ContainerInfo &containerInfo)
{
    char buf[PTY_BUFFER_SIZE];
    memset(buf, 0, sizeof(buf));

    ssize_t ret;
    while (true)
    {
        //AI_LOG_INFO("About to read from %d", containerInfo.pttyFd);
        ret = read(containerInfo.pttyFd, buf, sizeof(buf));
        // AI_LOG_INFO("Read %lu bytes", ret);
        if (ret <= 0)
        {
            break;
        }

        write(mJournaldSteamFd, buf, ret);
    }
}

void JournaldSink::SetContainerInfo(IDobbyRdkLoggingPlugin::ContainerInfo &containerInfo)
{
    // TODO:: this is crap
    mContainerInfo = containerInfo;
}

void JournaldSink::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
    // Got some data, yay
    if (events & EPOLLIN)
    {
        ssize_t ret;
        char buf[PTY_BUFFER_SIZE];
        bzero(&buf, sizeof(buf));

        ret = TEMP_FAILURE_RETRY(read(mContainerInfo.pttyFd, buf, sizeof(buf)));

        write(mJournaldSteamFd, buf, ret);

        return;
    }

    // Container shutdown
    if (events & EPOLLHUP)
    {
        AI_LOG_INFO("EPOLLHUP! Removing ourselves from the event loop!");
        // Remove ourselves from the event loop
        pollLoop->delSource(shared_from_this());

        // Clean up
        if (close(mContainerInfo.pttyFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container ptty fd");
        }

        if (close(mContainerInfo.connectionFd) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "Failed to close container connection");
        }

        return;
    }

    // Don't handle any other events
    return;
}