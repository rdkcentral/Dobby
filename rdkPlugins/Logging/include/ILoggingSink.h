#pragma once

#include "IPollLoop.h"
#include "IDobbyRdkLoggingPlugin.h"

class ILoggingSink : public AICommon::IPollSource,
                     public std::enable_shared_from_this<ILoggingSink>
{

public:
    virtual void DumpLog(const int bufferFd, const bool startNewLog) = 0;

    virtual void SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions& options) = 0;
};