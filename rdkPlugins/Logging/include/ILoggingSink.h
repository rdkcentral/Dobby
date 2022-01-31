#pragma once

#include "IPollLoop.h"
#include "IDobbyRdkLoggingPlugin.h"

#define PTY_BUFFER_SIZE 4096

class ILoggingSink : public AICommon::IPollSource,
                     public std::enable_shared_from_this<ILoggingSink>
{

public:
    virtual void DumpLog(const int bufferFd) = 0;

    virtual void SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions& options) = 0;
};