#pragma once

#include "ILoggingSink.h"

#define PTY_BUFFER_SIZE 4096

class FileSink : public ILoggingSink
{
public:
    FileSink(const std::string &containerId, std::shared_ptr<rt_dobby_schema> &containerConfig);

    ~FileSink();

public:
    void DumpLog(const int bufferFd, const bool startNewLog) override;

    void SetLogOptions(const IDobbyRdkLoggingPlugin::LoggingOptions &options) override;

    void process(const std::shared_ptr<AICommon::IPollLoop> &pollLoop, uint32_t events) override;

private:
    int openFile();
    void closeFile();

private:
    const std::shared_ptr<rt_dobby_schema> mContainerConfig;
    const std::string mContainerId;
    IDobbyRdkLoggingPlugin::LoggingOptions mLoggingOptions;

    ssize_t mFileSizeLimit;
    int mOutputFileFd;
    int mDevNullFd;
};