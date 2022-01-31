#pragma once

#include "ILoggingSink.h"

class NullSink : public ILoggingSink
{
public:
    NullSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig);
    ~NullSink();

public:
    void DumpLog(const int bufferFd) override;

    void SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions& options) override;

    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;

private:
    int mDevNullFd;
    const std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mContainerId;
    IDobbyRdkLoggingPlugin::LoggingOptions mLoggingOptions;

    char mBuf[PTY_BUFFER_SIZE];

    std::mutex mLock;
};