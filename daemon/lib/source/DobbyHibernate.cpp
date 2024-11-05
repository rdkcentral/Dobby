/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2016 Sky UK
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
/*
 * File:   DobbyHibernate.cpp
 *
 */


#include "DobbyHibernate.h"
#include <Logging.h>

#ifdef DOBBY_HIBERNATE_MEMCR_IMPL

#include <arpa/inet.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

typedef enum {
    MEMCR_CHECKPOINT = 100,
    MEMCR_RESTORE
} ServerRequestCode;

typedef enum {
    MEMCR_OK = 0,
    MEMCR_ERROR = -1,
    MEMCR_INVALID_PID = -2,
    MEMCR_SOCKET_READ_ERROR = -3
} ServerResponseCode;

typedef struct {
    ServerRequestCode reqCode;
    pid_t pid;
} __attribute__((packed)) ServerRequest;

typedef struct {
    ServerResponseCode respCode;
} __attribute__((packed)) ServerResponse;

const char* DobbyHibernate::DFL_LOCATOR = "/tmp/memcrcom";
const uint32_t DobbyHibernate::DFL_TIMEOUTE_MS = 20000;


static int Connect(const char* serverLocator, uint32_t timeoutMs)
{
    AI_LOG_FN_ENTRY();
    int cd = -1;
    struct sockaddr_in addrIn = { 0 };
    struct sockaddr_un addrUn = { 0 };
    struct sockaddr* addr = NULL;
    size_t addrSize = 0;
    char host[64] = { 0 };
    char* port = NULL;
    struct timeval rcvTimeout = { 0 };

    rcvTimeout.tv_sec = timeoutMs / 1000;
    rcvTimeout.tv_usec = (timeoutMs % 1000) * 1000;

    // check if we have unix or tcp socket
    if (strlen(serverLocator) == 0) {
        AI_LOG_ERROR("Locator empty");
        AI_LOG_FN_EXIT();
        return -1;
    }

    if (serverLocator[0] == '/') {
        // try to configure unix socket
        cd = socket(PF_UNIX, SOCK_STREAM, 0);
        if (cd < 0) {
            AI_LOG_ERROR("Unix Socket create failed: %d", cd);
            AI_LOG_FN_EXIT();
            return false;
        }

        addrUn.sun_family = PF_UNIX;
        strncpy(addrUn.sun_path, serverLocator, sizeof(addrUn.sun_path));
        addr = (struct sockaddr*)&addrUn;
        addrSize = sizeof(struct sockaddr_un);
    } else {
        // go on with inet socket
        cd = socket(AF_INET, SOCK_STREAM, 0);
        if (cd < 0) {
            AI_LOG_ERROR("Inet Socket create failed");
            AI_LOG_FN_EXIT();
            return -1;
        }

        strncpy(host, serverLocator, 64);
        port = strstr(host, ":");
        if (port == NULL) {
            AI_LOG_ERROR("Invalid Server Ip Address: %s", host);
            AI_LOG_FN_EXIT();
            return false;
        }

        // Add NULL delimer between host and port
        *port = 0;
        port++;

        addrIn.sin_family = AF_INET;
        addrIn.sin_addr.s_addr = inet_addr(host);
        addrIn.sin_port = htons(atoi(port));

        addr = (struct sockaddr*)&addrIn;
        addrSize = sizeof(struct sockaddr_in);
    }

    setsockopt(cd, SOL_SOCKET, SO_RCVTIMEO, &rcvTimeout, sizeof(rcvTimeout));

    int ret = connect(cd, addr, addrSize);
    if (ret < 0) {
        AI_LOG_ERROR("Socket connect failed: %d with %s", ret, serverLocator);
        close(cd);
        AI_LOG_FN_EXIT();
        return -1;
    }

    AI_LOG_FN_EXIT();
    return cd;
}

static bool SendRcvCmd(const ServerRequest* cmd, ServerResponse* resp, uint32_t timeoutMs, const char* serverLocator)
{
    AI_LOG_FN_ENTRY();
    int cd;
    int ret;
    resp->respCode = MEMCR_ERROR;

    cd = Connect(serverLocator, timeoutMs);
    if (cd < 0) {
        AI_LOG_ERROR("Unnable to connect to %s", serverLocator);
        AI_LOG_FN_EXIT();
        return false;
    }

    ret = write(cd, cmd, sizeof(ServerRequest));
    if (ret != sizeof(ServerRequest)) {
        AI_LOG_ERROR("Socket write failed: ret %d, %m", ret);
        close(cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    ret = read(cd, resp, sizeof(ServerResponse));
    if (ret != sizeof(ServerResponse)) {
        AI_LOG_ERROR("Socket read failed: ret %d, %m", ret);
        resp->respCode = MEMCR_SOCKET_READ_ERROR;
        close(cd);
        AI_LOG_FN_EXIT();
        return false;
    }

    close(cd);

    AI_LOG_FN_EXIT();
    return (resp->respCode == MEMCR_OK);
}

DobbyHibernate::Error DobbyHibernate::HibernateProcess(const pid_t pid, const uint32_t timeout,  const char* locator,
    const char* dumpDirPath, CompressionAlg compression)
{
    AI_LOG_FN_ENTRY();
    ServerRequest req = {
        .reqCode = MEMCR_CHECKPOINT,
        .pid = pid
    };
    ServerResponse resp;

    if (SendRcvCmd(&req, &resp, timeout, locator)) {
        AI_LOG_INFO("Hibernate process PID %d success", pid);
        AI_LOG_FN_EXIT();
        return DobbyHibernate::Error::ErrorNone;
    } else if (resp.respCode == MEMCR_SOCKET_READ_ERROR) {
        AI_LOG_WARN("Error Hibernate timeout process PID %d ret %d", pid, resp.respCode);
        AI_LOG_FN_EXIT();
        return DobbyHibernate::Error::ErrorTimeout;
    } else {
        AI_LOG_WARN("Error Hibernate process PID %d ret %d", pid, resp.respCode);
        AI_LOG_FN_EXIT();
        return DobbyHibernate::Error::ErrorGeneral;
    }
}

DobbyHibernate::Error DobbyHibernate::WakeupProcess(const pid_t pid, const uint32_t timeout, const char* locator)
{
    AI_LOG_FN_ENTRY();
    ServerRequest req = {
        .reqCode = MEMCR_RESTORE,
        .pid = pid
    };
    ServerResponse resp;

    if (SendRcvCmd(&req, &resp, timeout, locator)) {
        AI_LOG_INFO("Wakeup process PID %d success", pid);
        AI_LOG_FN_EXIT();
        return DobbyHibernate::Error::ErrorNone;
    } else if (resp.respCode == MEMCR_INVALID_PID) {
        AI_LOG_WARN("Wakeup process PID %d ret %d - INVALID PID, nothing to wakeup", pid, resp.respCode);
        AI_LOG_FN_EXIT();
        return DobbyHibernate::Error::ErrorNone;
    }

    AI_LOG_WARN("Error Wakeup process PID %d ret %d", pid, resp.respCode);
    AI_LOG_FN_EXIT();
    return DobbyHibernate::Error::ErrorGeneral;
}

#else

const char* DobbyHibernate::DFL_LOCATOR = "";
const uint32_t DobbyHibernate::DFL_TIMEOUTE_MS = 0;

DobbyHibernate::Error DobbyHibernate::HibernateProcess(const pid_t pid, const uint32_t timeout,  const char* locator,
    const char* dumpDirPath, CompressionAlg compression)
{
    AI_LOG_ERROR("DobbyHibernate Implementation not enabled");
    return DobbyHibernate::Error::ErrorGeneral;
}

DobbyHibernate::Error DobbyHibernate::WakeupProcess(const pid_t pid, const uint32_t timeout, const char* locator)
{
    AI_LOG_ERROR("DobbyHibernate Implementation not enabled");
    return DobbyHibernate::Error::ErrorGeneral;
}

#endif