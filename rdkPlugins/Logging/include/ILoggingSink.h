#pragma once

#include "IPollLoop.h"
#include "IDobbyRdkLoggingPlugin.h"

class ILoggingSink : public AICommon::IPollSource,
                     public std::enable_shared_from_this<ILoggingSink>
{

public:
    virtual void DumpLog(const IDobbyRdkLoggingPlugin::ContainerInfo &containerInfo) = 0;

    virtual void SetContainerInfo(IDobbyRdkLoggingPlugin::ContainerInfo &containerInfo) = 0;
};