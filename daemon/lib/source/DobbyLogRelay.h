#pragma once

#include "IPollLoop.h"

#include <sys/socket.h>
#include <sys/un.h>

#define BUFFER_SIZE (16 * 1024)

class DobbyLogRelay : public AICommon::IPollSource,
                      public std::enable_shared_from_this<DobbyLogRelay>
{
public:
    DobbyLogRelay(const int sourceSocketFd,
                  const std::string &destinationSocketPath);

public:
    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;

private:
    const int mSourceSocketFd;
    const std::string mDestinationSocketPath;

    int mDestinationSocketFd;

    sockaddr_un mDestinationSocketAddress;
};