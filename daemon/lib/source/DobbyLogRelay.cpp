#include "DobbyLogRelay.h"

#include <unistd.h>
#include <strings.h>
#include <fcntl.h>
#include <map>
#include <sys/stat.h>
#include <sys/un.h>

#include <Logging.h>

DobbyLogRelay::DobbyLogRelay(const int sourceSocketFd, const std::string &destinationSocketPath)
      : mSourceSocketFd(sourceSocketFd),
      mDestinationSocketPath(destinationSocketPath)
{
    AI_LOG_FN_ENTRY();

    // Set up the socket we will relay messages to
    mDestinationSocketAddress = {};
    mDestinationSocketFd = socket(AF_UNIX, SOCK_DGRAM, 0);
    mDestinationSocketAddress.sun_family = AF_UNIX;
    strcpy(mDestinationSocketAddress.sun_path, mDestinationSocketPath.c_str());

    AI_LOG_INFO("Created log relay from fd %d to %d", sourceSocketFd, mDestinationSocketFd);

    AI_LOG_FN_EXIT();
}

void DobbyLogRelay::process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events)
{
    if (events & EPOLLIN)
    {
        char buf[BUFFER_SIZE] = {};
        ssize_t ret;

        ret = TEMP_FAILURE_RETRY(read(mSourceSocketFd, buf, sizeof(buf)));

        if (ret < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Errror during read");
        }

        if (sendto(mDestinationSocketFd, buf, sizeof(buf), 0, (struct sockaddr *)&mDestinationSocketAddress, sizeof(mDestinationSocketAddress)) < 0)
        {
            AI_LOG_SYS_ERROR(errno, "sento failed");
        }
    }

    return;
}