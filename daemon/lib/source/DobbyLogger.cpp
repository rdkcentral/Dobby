/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2020 Sky UK
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
* http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*/
#include "DobbyLogger.h"

#include <Logging.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <future>
#include <chrono>

DobbyLogger::DobbyLogger(const std::shared_ptr<const IDobbySettings> &settings)
    : mSocketPath(settings->consoleSocketPath()),
      mSyslogSocketPath(settings->logRelaySettings().syslogSocketPath),
      mJournaldSocketPath(settings->logRelaySettings().journaldSocketPath),
      mSyslogFd(-1),
      mJournaldFd(-1),
      mShutdown(false),
      mPollLoop(std::make_shared<AICommon::PollLoop>("DobbyLogger"))
{
    AI_LOG_FN_ENTRY();

    // Create the socket that crun will connect to
    mSocketFd = createUnixSocket(mSocketPath);
    if (mSocketFd > 0)
    {
        AI_LOG_INFO("Logging socket created at '%s'", mSocketPath.c_str());
    }
    else
    {
        AI_LOG_ERROR("Failed to create logging socket");
    }

    auto logRelaySettings = settings->logRelaySettings();

    if (logRelaySettings.syslogEnabled)
    {
        if (!mSyslogSocketPath.empty())
        {
            mSyslogFd = createDgramSocket(mSyslogSocketPath);
            if (mSyslogFd < 0)
            {
                AI_LOG_ERROR("Failed to create syslog relay socket");
            }
            else
            {
                mSyslogRelay = std::make_unique<DobbyLogRelay>(mSyslogFd, "/dev/log");
                mPollLoop->addSource(mSyslogRelay, mSyslogFd, EPOLLIN);
            }
        }
        else
        {
            AI_LOG_WARN("Syslog relay enabled but no socket path set in settings");
        }
    }

    if (logRelaySettings.journaldEnabled)
    {
        if (!mJournaldSocketPath.empty())
        {
            mJournaldFd = createDgramSocket(mJournaldSocketPath);
            if (mJournaldFd < 0)
            {
                AI_LOG_ERROR("Failed to create journald relay socket");
            }
            else
            {
                mJournaldRelay = std::make_unique<DobbyLogRelay>(mJournaldFd, "/run/systemd/journal/socket");
                mPollLoop->addSource(mJournaldRelay, mJournaldFd, EPOLLIN);
            }
        }
        else
        {
            AI_LOG_WARN("Journald relay enabled but no socket path set in settings");
        }
    }

    // Start poll loop
    mPollLoop->start();

    // Monitor that socket for new connections
    std::thread socketConnectionMonitor(&DobbyLogger::connectionMonitorThread, this, mSocketFd);
    socketConnectionMonitor.detach();

    AI_LOG_FN_EXIT();
}

DobbyLogger::~DobbyLogger()
{
    AI_LOG_FN_ENTRY();

    mShutdown = true;

    // Stop the poll loop running
    mPollLoop->delAllSources();
    mPollLoop->stop();

    //  Close all our open sockets
    if (shutdown(mSocketFd, SHUT_RDWR) < 0)
    {
        AI_LOG_SYS_WARN(errno, "Failed to shutdown socket %d", mSocketFd);
    }

    if (close(mSocketFd) < 0)
    {
        AI_LOG_SYS_WARN(errno, "Failed to close socket %d", mSocketFd);
    }

    if (mSyslogFd > 0 && close(mSyslogFd) < 0)
    {
        AI_LOG_SYS_WARN(errno, "Failed to close socket %d", mSyslogFd);
    }

    if (mJournaldFd > 0 && close(mJournaldFd) < 0)
    {
        AI_LOG_SYS_WARN(errno, "Failed to close socket %d", mJournaldFd);
    }

    // Clean up when we're done
    if (unlink(mSocketPath.c_str()))
    {
        AI_LOG_SYS_ERROR(errno, "Failed to remove socket at '%s'", mSocketPath.c_str());
    }

    if (!mJournaldSocketPath.empty() && unlink(mJournaldSocketPath.c_str()))
    {
        AI_LOG_SYS_ERROR(errno, "Failed to remove socket at '%s'", mJournaldSocketPath.c_str());
    }

    if (!mSyslogSocketPath.empty() && unlink(mSyslogSocketPath.c_str()))
    {
        AI_LOG_SYS_ERROR(errno, "Failed to remove socket at '%s'", mSyslogSocketPath.c_str());
    }


    AI_LOG_FN_EXIT();
}

int DobbyLogger::createDgramSocket(const std::string &path)
{
    // Remove the socket if it exists already...
    unlink(path.c_str());

    // Create a socket
    int sockFd;
    sockFd = socket(AF_UNIX, SOCK_DGRAM, 0);

    struct sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path.c_str());

    if (bind(sockFd, (const struct sockaddr *)&address, sizeof(address)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to bind socket @ '%s'", address.sun_path);
        return -1;
    }

    // We want to be able to extract credentials
    int optval = 1;
    if (setsockopt(sockFd, SOL_SOCKET, SO_PASSCRED, &optval, sizeof(optval)) < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "Failed to setsockopt on '%s'", address.sun_path);
        return -1;
    }

    chmod(path.c_str(), S_IRWXU | S_IRWXG | S_IRWXO);
    return sockFd;
}


/**
 * @brief Create a new UNIX domain socket that the OCI runtime can connect to
 * and send the fd of the ptty used for the container
 *
 * @param[in] path  Where to create the socket
 */
int DobbyLogger::createUnixSocket(const std::string path)
{
    AI_LOG_FN_ENTRY();

    // This will probably fail since the socket should be deleted when Dobby
    // exits, but in case of crash clean up before we start
    unlink(mSocketPath.c_str());

    // Create a file descriptor for the socket
    int sockFd;
    sockFd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Could not create new UNIX socket");
        return -1;
    }

    // Set properties on the socket
    struct sockaddr_un address = {};
    address.sun_family = AF_UNIX;
    strcpy(address.sun_path, path.c_str());

    // Attempt to bind the socket
    if (TEMP_FAILURE_RETRY(bind(sockFd, (const struct sockaddr *)&address, sizeof(address))) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to bind socket");
        return -1;
    }

    // Put the socket into listening state ready to accept connections
    if (listen(sockFd, 1) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Cannot set listen mode on socket");
        return -1;
    }

    AI_LOG_FN_EXIT();
    return sockFd;
}

/**
 * @brief Once a connection to the socket has been made, wait to receive a
 * message that contains a file descriptor
 *
 * @param[in] connectionFd  fd of the connection to the Dobby logging socket
 */
int DobbyLogger::receiveFdFromSocket(const int connectionFd)
{
    // We don't use the data buffer for this, but we need one (even if it's empty)
    char dataBuffer[1];

    // Linux uses this ancillary data mechanism to pass file descriptors over
    // UNIX domain sockets, so this is what we're interested in
    char ancillaryDataBuffer[CMSG_SPACE(sizeof(int))] = {};

    struct iovec iov[1];
    iov[0].iov_base = dataBuffer;
    iov[0].iov_len = sizeof(dataBuffer);

    struct msghdr msg = {};
    msg.msg_name = NULL;
    msg.msg_namelen = 0;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;
    msg.msg_control = ancillaryDataBuffer;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));

    ssize_t messageSize = 0;

    // Block waiting to receive a message over the open connection
    messageSize = TEMP_FAILURE_RETRY(recvmsg(connectionFd, &msg, 0));

    if (messageSize < 0)
    {
        AI_LOG_SYS_WARN(errno, "Something went wrong receiving the message from the socket");
        return -1;
    }

    // Extract the data from the ancillary data
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == nullptr)
    {
        AI_LOG_WARN("Received message was null - container terminal is likely set to \"false\"");
        return -1;
    }

    // We expect a specific message type and level
    if (cmsg->cmsg_type != SCM_RIGHTS || cmsg->cmsg_level != SOL_SOCKET)
    {
        AI_LOG_WARN("Received unexpected message");
        return -1;
    }

    // Get the fd sent by crun
    int stdioFd;
    memcpy(&stdioFd, CMSG_DATA(cmsg), sizeof(stdioFd));
    return stdioFd;
}

/**
 * @brief Runs for the lifetime of the daemon, waiting for new connections
 * to the socket. Once a connection is received, add to the map
 *
 * @param[in] socketFd  FD of the socket passed to the runtime --console-socket param
 */
void DobbyLogger::connectionMonitorThread(const int socketFd)
{
    AI_LOG_FN_ENTRY();

    pthread_setname_np(pthread_self(), "DOBBY_LOG_MON");

    AI_LOG_INFO("Dobby Logger socket monitoring thread started");

    if (socketFd < 0)
    {
        AI_LOG_ERROR("Logging socket fd is invalid");
        return;
    }

    while (true)
    {
        // This will block until we have a connection
        int connection = TEMP_FAILURE_RETRY(accept(socketFd, NULL, NULL));
        if (connection < 0)
        {
            // If we're shutting down, this is expected
            if (mShutdown)
            {
                return;
            }
            else
            {
                AI_LOG_SYS_ERROR(errno, "Error accepting connection");
                break;
            }

        }

        struct ucred conCredentials = {};
        socklen_t length = sizeof(struct ucred);

        if (getsockopt(connection, SOL_SOCKET, SO_PEERCRED, &conCredentials, &length) < 0)
        {
            AI_LOG_WARN("Could not retrieve connection credentials - cannot determine connection PID to match logs with container");
            break;
        }

        int containerStdioFd = receiveFdFromSocket(connection);
        if (containerStdioFd < 0)
        {
            AI_LOG_INFO("Couldn't extract FD from message - closing connection");
            close(connection);
            continue;
        }

        // Set the correct carriage return settings
        struct termios containerIo = {};
        if (tcgetattr(containerStdioFd, &containerIo) == -1)
        {
            // Failed to get, but carry on regardless. Might just result in some
            // weird formatting
            AI_LOG_SYS_WARN(errno, "Failed to get container terminal settings");
        }
        else
        {
            // Set terminal to output only \n instead of \r\n
            containerIo.c_oflag = (OPOST | OCRNL);
            if (tcsetattr(containerStdioFd, TCSANOW, &containerIo) == -1)
            {
                AI_LOG_SYS_WARN(errno, "Could not update container terminal settings");
            }
        }

        std::lock_guard<std::mutex> locker(mLock);

        IDobbyRdkLoggingPlugin::LoggingOptions info{
            -1, // Unknown at this point as container isn't running
            connection,
            containerStdioFd};

        mTempConnections.insert(std::make_pair(conCredentials.pid, info));
        AI_LOG_INFO("New logging socket connection from PID %d", conCredentials.pid);
    }

    AI_LOG_FN_EXIT();
}


/**
 * @brief Public method that should be called once a container has been created
 * to match the container PID with the runtime PID to start running the logging
 * thread based on whichever logging plugin is loaded
 *
 * @param[in] containerId   Name of the container
 * @param[in] runtimePid    PID of the OCI runtime that connected to the socket
 * @param[in] containerPid  PID of the running container
 * @param[in] loggingPlugin Plugin that will process the container logs
 * @param[in] createNewLog  If true, create a new, empty logfile if applicable
 *
 * @return True if thread started successfully
 */
bool DobbyLogger::StartContainerLogging(std::string containerId,
                                        pid_t runtimePid,
                                        pid_t containerPid,
                                        std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin,
                                        const bool createNewLog)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    AI_LOG_INFO("Configuring logging for container '%s' (pid: %d)", containerId.c_str(), containerPid);

    // Is the console socket connected?
    auto it = mTempConnections.find(runtimePid);
    if (it == mTempConnections.end())
    {
        AI_LOG_WARN("Cannot configure logging for container %s - not connected to socket", containerId.c_str());
        return false;
    }

    it->second.containerPid = containerPid;
    it->second.createNewLog = createNewLog;

    if (!loggingPlugin)
    {
        AI_LOG_WARN("Logging plugin is null");
        return false;
    }

    // Logging plugin should now register poll sources on the epoll loop
    loggingPlugin->RegisterPollSources(it->second, mPollLoop);

    // Thread is up and running, don't need to track the connection any more
    // as thread can detect when the container closes and clean up
    mTempConnections.erase(it);


    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Blocking method that writes the contents of a buffer at a given memFd
 * to the logger specified in the container config.
 *
 * Mainly used for writing the contents of the OCI hooks stdout/err to the
 * container logfile
 *
 * @param[in] bufferMemFd   fd of the buffer
 * @param[in] containerPid  PID of the container the logs belong to
 * @param[in] loggingPluing The logging plugin the contents of the buffer should
 *                          be sent to
 */
bool DobbyLogger::DumpBuffer(int bufferMemFd,
                             pid_t containerPid,
                             std::shared_ptr<IDobbyRdkLoggingPlugin> loggingPlugin,
                             const bool createNewLog)
{
    AI_LOG_FN_ENTRY();

    std::lock_guard<std::mutex> locker(mLock);

    // Make sure we seek the buffer to the start so we can read from it later
    if (lseek(bufferMemFd, 0, SEEK_SET) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to seek to beginning of memfd");
        return false;
    }

    // Actually do the logging
    if (!loggingPlugin)
    {
        AI_LOG_WARN("Logging plugin is null");
        return false;
    }

    loggingPlugin->DumpToLog(bufferMemFd, createNewLog);

    AI_LOG_FN_EXIT();
    return true;
}