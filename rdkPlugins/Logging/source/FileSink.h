#pragma once

#include "ILoggingSink.h"

class FileSink : public ILoggingSink
{
public:
    FileSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig);

    ~FileSink();

public:
    void DumpLog(const int bufferFd) override;

    void SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions &options) override;

    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;

private:
    int openFile();

private:
    const std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mContainerId;
    IDobbyRdkLoggingPlugin::LoggingOptions mLoggingOptions;

    ssize_t mFileSizeLimit;
    int mOutputFileFd;
    int mDevNullFd;

    bool mLimitHit;
    char mBuf[PTY_BUFFER_SIZE];

    std::mutex mLock;
};