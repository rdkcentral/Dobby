/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2021 Sky UK
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

#include "AnonymousFile.h"
#include "Logging.h"

#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

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

/**
 * @brief Constructor - called when content of already created volatile file matters
 *
 * @param[in]  fd  file descriptor to previously created volatile file
 */
AnonymousFile::AnonymousFile(int fd /*= -1*/) : mFd(fd)
{
    AI_LOG_FN_ENTRY();
    AI_LOG_FN_EXIT();
}

/**
 * @brief Creates a volatile file that lives in RAM
 *
 * @return File descriptor to a volatile file on success or -1 on failure
 */
int AnonymousFile::create()
{
    AI_LOG_FN_ENTRY();

    if (mFd != -1)
    {
        AI_LOG_FN_EXIT();
        return mFd;
    }

    // did some testing and it turns out that data written to memfd is accounted to the container
    // it is allowed to not truncate and not limit file growing using seals at this point
    mFd = memfd_create("anon_file", MFD_CLOEXEC);
    if (mFd == -1)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to create anonymous file");
        return -1;
    }

    AI_LOG_FN_EXIT();
    return mFd;
}

/**
 * @brief Copies content of volatile file from RAM to a disk
 *
 * @param[in]  destFile  destination file path
 *
 * @return True on success or false on failure
 */
bool AnonymousFile::copyContentTo(const std::string& destFile)
{
    AI_LOG_FN_ENTRY();

    if (mFd == -1)
    {
        AI_LOG_ERROR_EXIT("Incorrect fd provided: %d", mFd);
        return false;
    }

    // it turns out that fclose(fp) will do effectively the same job as close(fd)
    // therefore this guy will not be fclose'd in here, but rather reset by nullptr value
    // mind that closing related fd will be accomplished by DobbyStartState destructor
    // so this is totally fine from this class PoV
    auto fp = fdopen(mFd, "r");
    if (!fp)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to open fd %d for reading", mFd);
        return false;
    }

    long fileSize = getFileSize(fp);
    if (fileSize <= 0)
    {
        AI_LOG_WARN("Bad minidump detected before cleanup (reason=empty_or_invalid_size, fd=%d, size=%ld, dest=%s)",
                    mFd,
                    fileSize,
                    destFile.c_str());
        AI_LOG_DEBUG("Empty or invalid file size %ld for fd %d", fileSize, mFd);
        fclose(fp);
        fp = nullptr;
        AI_LOG_FN_EXIT();
        return false;
    }

    char* buffer = (char*) malloc(sizeof(char) * (fileSize + 1));
    if (!buffer)
    {
        AI_LOG_SYS_ERROR_EXIT(errno, "failed to allocate buffer for reading fd %d", mFd);
        fclose(fp);
        fp = nullptr;
        return false;
    }

    size_t elementsRead = fread(buffer, 1, fileSize, fp);
    if (elementsRead != fileSize)
    {
        AI_LOG_WARN("Bad minidump detected before cleanup (reason=short_read, fd=%d, expected=%ld, actual=%zu, dest=%s)",
                    mFd,
                    fileSize,
                    elementsRead,
                    destFile.c_str());
        AI_LOG_ERROR_EXIT("failed to read fd %d correctly", mFd);
        fclose(fp);
        fp = nullptr;
        free(buffer);
        return false;
    }

    buffer[fileSize] = '\0';

    // check file header
    if (fileSize < 4 || strncmp(buffer, "MDMP", 4) != 0)
    {
        const unsigned char b0 = (fileSize > 0) ? static_cast<unsigned char>(buffer[0]) : 0;
        const unsigned char b1 = (fileSize > 1) ? static_cast<unsigned char>(buffer[1]) : 0;
        const unsigned char b2 = (fileSize > 2) ? static_cast<unsigned char>(buffer[2]) : 0;
        const unsigned char b3 = (fileSize > 3) ? static_cast<unsigned char>(buffer[3]) : 0;
        AI_LOG_WARN("Bad minidump detected before cleanup (reason=invalid_header, fd=%d, size=%ld, hdr=%02X%02X%02X%02X, dest=%s)",
                    mFd,
                    fileSize,
                    b0,
                    b1,
                    b2,
                    b3,
                    destFile.c_str());
        AI_LOG_WARN("Incorrect file header for fd %d", mFd);
        fclose(fp);
        fp = nullptr;
        free(buffer);
        AI_LOG_FN_EXIT();
        return false;
    }

    int destFd = open(destFile.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0666);
    if (destFd == -1)
    {
        AI_LOG_ERROR_EXIT("Cannot open %s", destFile.c_str());
        fclose(fp);
        fp = nullptr;
        free(buffer);
        return false;
    }

    // Minidumps are binary files; write exactly fileSize bytes and handle partial writes.
    ssize_t totalWritten = 0;
    while (totalWritten < static_cast<ssize_t>(fileSize))
    {
        const ssize_t written = write(destFd,
                                      buffer + totalWritten,
                                      fileSize - totalWritten);
        if (written < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }

            AI_LOG_WARN("Bad minidump detected before cleanup (reason=write_error, fd=%d, size=%ld, written=%zd, errno=%d, dest=%s)",
                        mFd,
                        fileSize,
                        totalWritten,
                        errno,
                        destFile.c_str());
            AI_LOG_SYS_ERROR(errno, "failed to write minidump to %s", destFile.c_str());
            fclose(fp);
            fp = nullptr;
            free(buffer);
            close(destFd);
            if (unlink(destFile.c_str()) == 0)
            {
                AI_LOG_INFO("Removed partial minidump after write failure: %s", destFile.c_str());
            }
            else
            {
                AI_LOG_WARN("Failed to remove partial minidump '%s' after write failure (errno=%d)",
                            destFile.c_str(),
                            errno);
            }
            return false;
        }

        if (written == 0)
        {
            AI_LOG_WARN("Bad minidump detected before cleanup (reason=write_zero_progress, fd=%d, size=%ld, written=%zd, dest=%s)",
                        mFd,
                        fileSize,
                        totalWritten,
                        destFile.c_str());
            AI_LOG_ERROR("write returned 0 while writing minidump to %s", destFile.c_str());
            fclose(fp);
            fp = nullptr;
            free(buffer);
            close(destFd);
            if (unlink(destFile.c_str()) == 0)
            {
                AI_LOG_INFO("Removed partial minidump after zero-progress write: %s", destFile.c_str());
            }
            else
            {
                AI_LOG_WARN("Failed to remove partial minidump '%s' after zero-progress write (errno=%d)",
                            destFile.c_str(),
                            errno);
            }
            return false;
        }

        totalWritten += written;
    }

    fclose(fp);

    fp = nullptr;
    free(buffer);
    close(destFd);

    AI_LOG_INFO("Minidump copied to: %s", destFile.c_str());

    AI_LOG_FN_EXIT();
    return true;
}

/**
 * @brief Calculates file size for provided file pointer
 *
 * Please note that file position indicator is reset to begining
 *
 * @param[in]  fp  file pointer for which size calculation will happen
 *
 * @return File size
 */
long AnonymousFile::getFileSize(FILE* fp)
{
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    return size;
}
