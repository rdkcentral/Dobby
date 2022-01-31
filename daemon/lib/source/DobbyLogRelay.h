#pragma once

#include "IPollLoop.h"

#include <sys/socket.h>
#include <sys/un.h>

#define BUFFER_SIZE (16 * 1024)

class DobbyLogRelay : public AICommon::IPollSource,
                      public std::enable_shared_from_this<DobbyLogRelay>
{
public:
    DobbyLogRelay(const std::string &sourceSocketPath,
                  const std::string &destinationSocketPath);
    ~DobbyLogRelay();

public:
    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;
    int getSourceFd();

private:
    int createDgramSocket(const std::string& path);

private:
    const std::string mSourceSocketPath;
    const std::string mDestinationSocketPath;

    int mSourceSocketFd;
    int mDestinationSocketFd;

    sockaddr_un mDestinationSocketAddress;
};