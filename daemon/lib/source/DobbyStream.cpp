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

#include "DobbyStream.h"

#include <Logging.h>

#include <stdio.h>
#include <poll.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/eventfd.h>


// some platforms are missing the memfd headers
// #include <linux/memfd.h>
#if !defined(MFD_CLOEXEC)
#  define MFD_CLOEXEC           0x0001U
#endif
#if !defined(MFD_ALLOW_SEALING)
#  define MFD_ALLOW_SEALING     0x0002U
#endif

// for some reason the XiOne toolchain is build against old kernel headers
// which doesn't have the memfd syscall
#if !defined(SYS_memfd_create)
#  if defined(__arm__)
#    define SYS_memfd_create    385
#  elif defined _MIPS_SIM
#    if _MIPS_SIM == _MIPS_SIM_ABI32
#      define SYS_memfd_create 4354
#    endif
#    if _MIPS_SIM == _MIPS_SIM_NABI32
#      define SYS_memfd_create 6318
#    endif
#    if _MIPS_SIM == _MIPS_SIM_ABI64
#      define SYS_memfd_create 5314
#    endif
#  endif
#endif

// glibc prior to version 2.27 didn't have a syscall wrapper for memfd_create(...)
#if defined(__GLIBC__) && ((__GLIBC__ < 2) || ((__GLIBC__ >= 2) && (__GLIBC_MINOR__ < 27)))
#include <syscall.h>
static inline int memfd_create(const char *name, unsigned int flags)
{
    return syscall(SYS_memfd_create, name, flags);
}
#endif


// -----------------------------------------------------------------------------
/**
 *  @brief Returns a dup'd file descriptor for the write side of the stream
 *
 *  If the file descriptor @a newfd was previously open, it is silently closed
 *  before being reused.  If @a newFd is -1 then the lowest-numbered unused
 *  file descriptor number is used.
 *
 *  @param[in]  newFd       The number to give the new file descriptor.
 *  @param[in]  closeExec   If true the O_CLOEXEC flag is set on the new fd
 *
 *  @return the new file descriptor, on error -1.
 */
int DobbyDevNullStream::dupWriteFD(int newFd, bool closeExec) const
{
    AI_LOG_FN_ENTRY();

    int flags = O_WRONLY;
    if (closeExec)
        flags |= O_CLOEXEC;

    int fd = open("/dev/null", flags);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open /dev/null");
        return -1;
    }

    if (newFd >= 0)
    {
        if (closeExec)
            newFd = dup3(fd, newFd, O_CLOEXEC);
        else
            newFd = dup2(fd, newFd);

        if (newFd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to dup /dev/null fd");
            newFd = -1;
        }

        close(fd);
        fd = newFd;
    }

    return fd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Constructs the buffer using an memfd.
 *
 *  @param[in]  limit       The maximum number of bytes that can be written into
 *                          the buffer.
 *
 */
DobbyBufferStream::DobbyBufferStream(ssize_t limit)
    : mMemFd(-1)
{
    (void)limit;

    mMemFd = memfd_create("streambuffer", MFD_CLOEXEC);
    if (mMemFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create memfd");

        // so as to keep eveything ticking over, use /dev/null as a fake memfd
        // open /dev/null
        mMemFd = open("/dev/null", O_CLOEXEC | O_WRONLY);
        if (mMemFd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to open /dev/null");
        }
    }
}

// -----------------------------------------------------------------------------
/**
 *
 */
DobbyBufferStream::~DobbyBufferStream()
{
    if ((mMemFd >= 0) && (close(mMemFd) != 0))
    {
        AI_LOG_SYS_ERROR(errno, "failed to close memfd");
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Returns a dup'd file descriptor for the write side of the stream
 *
 *  If the file descriptor @a newfd was previously open, it is silently closed
 *  before being reused.  If @a newFd is -1 then the lowest-numbered unused
 *  file descriptor number is used.
 *
 *  @param[in]  newFd       The number to give the new dup'd file descriptor.
 *  @param[in]  closeExec   If true the O_CLOEXEC flag is set on the new fd
 *
 *  @return the new file descriptor, on error -1.
 */
int DobbyBufferStream::dupWriteFD(int newFd, bool closeExec) const
{
    // we don't use mMemFd number as we want to keep our local file pointers, so
    // instead open another copy of the memfd and give that to the caller
    char memfdPath[32];
    sprintf(memfdPath, "/proc/self/fd/%d", mMemFd);

    int fd = open(memfdPath, O_CLOEXEC | O_WRONLY | O_APPEND);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '%s'", memfdPath);
        return -1;
    }

    int dupFd = -1;

    if (closeExec)
    {
        if (newFd < 0)
            dupFd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
        else
            dupFd = dup3(fd, newFd, O_CLOEXEC);
    }
    else
    {
        if (newFd < 0)
            dupFd = dup(fd);
        else
            dupFd = dup2(fd, newFd);
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '%s'", memfdPath);
    }

    return dupFd;
}

// -----------------------------------------------------------------------------
/**
 *  @brief Reads all the data in the buffer.
 *
 *  This doesn't do a flush or anything, this just returns everything written
 *  into the buffer.
 *
 *
 */
std::vector<char> DobbyBufferStream::getBuffer() const
{
    off_t size = lseek(mMemFd, 0, SEEK_END);
    if (size < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to get memfd size");
        return {};
    }


    
    if (lseek(mMemFd, 0, SEEK_SET) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to seek to beginning of memfd");
        return {};
    }

    constexpr off_t kMaxSize = 1 * 1024 * 1024;
    size = std::min(size, kMaxSize);

    if (size < 0 || static_cast<uintmax_t>(size) > static_cast<uintmax_t>(std::numeric_limits<size_t>::max()))
    {
        AI_LOG_SYS_ERROR(errno, "invalid buffer size");
        return {};
    }

    std::vector<char> buf(static_cast<size_t>(size), 0x00);
    char* dataPtr = buf.data();
    size_t remaining = static_cast<size_t>(size);

    while (remaining > 0)
    {
        ssize_t rd = TEMP_FAILURE_RETRY(read(mMemFd, dataPtr, remaining));
        if (rd < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to read from memfd");
            break;
        }
        else if (rd == 0)
        {
            break;
        }
        else
        {
            if (static_cast<size_t>(rd) > remaining)
            {
                AI_LOG_SYS_ERROR(errno, "read returned more bytes than expected");
                break;
            }

            dataPtr += rd;
            remaining -= static_cast<size_t>(rd);
        }
    }
    size_t actualSize = static_cast<size_t>(size) - remaining;
    buf.resize(actualSize);

    return buf;
}

int DobbyBufferStream::getMemFd() const
{
    return mMemFd;
}
