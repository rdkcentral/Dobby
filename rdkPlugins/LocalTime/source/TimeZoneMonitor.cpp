/*
 * If not stated otherwise in this file or this component's LICENSE file the
 * following copyright and licenses apply:
 *
 * Copyright 2026 Sky UK
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

#include "TimeZoneMonitor.h"
#include <Logging.h>

#include <random>
#include <string_view>

#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/inotify.h>

#define MAX_TZ_FILE_SIZE 4096

TimeZoneMonitor::TimeZoneMonitor(std::filesystem::path timeZoneFilePath)
    : mTimeZoneFilePath(std::move(timeZoneFilePath))
    , mTimeZoneFileName(mTimeZoneFilePath.filename())
{
    AI_LOG_INFO("creating time zone monitor for '%s'", mTimeZoneFilePath.c_str());

    // the time zone file is typically a symlink to a file in /usr/share/zoneinfo.
    // However, in case it's not a symlink or the target symlink is not available
    // inside the container, then we copy that actual target file to the container
    // rootfs

    std::error_code ec;
    if (!std::filesystem::exists(mTimeZoneFilePath, ec))
    {
        AI_LOG_WARN("time zone file '%s' does not exist, waiting for it to appear",
                    mTimeZoneFilePath.c_str());
    }
    else
    {
        recheckTimeZoneFile();
    }

    mStopEventFd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (mStopEventFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create stop event fd");
        return;
    }

    mMonitorThread = std::thread(&TimeZoneMonitor::monitorLoop, this);
}

TimeZoneMonitor::~TimeZoneMonitor()
{
    if (mStopEventFd >= 0)
    {
        uint64_t value = 1;
        if (::write(mStopEventFd, &value, sizeof(value)) != sizeof(value))
            AI_LOG_SYS_ERROR(errno, "failed to write to stop event fd");
    }

    if (mMonitorThread.joinable())
        mMonitorThread.join();

    if ((mStopEventFd >= 0) && (close(mStopEventFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close stop event fd");
}

// -----------------------------------------------------------------------------
/**
 *  @brief Adds a path to a symlink file to be updated when the time zone file
 *  changes.
 *
 *  When a path is added it it automatically updated with the current target
 *  of the time zone file.  Then in the future if a change is detected to the
 *  time zone file then the registered paths, which are expected to be symlinks,
 *  are updated to point to the new target of the time zone file.
 *
 *  This function is thread safe.
 */
void TimeZoneMonitor::addPathToUpdate(const std::filesystem::path &path)
{
    std::lock_guard<std::mutex> lock(mLock);

    mPathsToUpdate.insert(path);

    if (!mTimeZoneFileContent.empty())
        updateTimeZoneFile(path, mTimeZoneFileContent);
}

// -----------------------------------------------------------------------------
/**
 *  @brief Removes a path from the list of paths to be updated when the time
 *  zone file changes.
 *
 *  This function is thread safe.
 */
void TimeZoneMonitor::removePathToUpdate(const std::filesystem::path &path)
{
    std::lock_guard<std::mutex> lock(mLock);
    mPathsToUpdate.erase(path);
}

// -----------------------------------------------------------------------------
/**
 *  @brief thread loop to monitor the time zone file for changes and update the
 *  symlink.
 *
 *  This is installs inotify watches on the time zone file and the parent
 *  directory.  Any changes will trigger a re-read of the time zone symlink and
 *  update the registered paths with the new target.
 */
void TimeZoneMonitor::monitorLoop()
{
    AI_LOG_INFO("starting time zone monitor thread for '%s'", mTimeZoneFilePath.c_str());

    pthread_setname_np(pthread_self(), "AI_TZ_MONITOR");

    std::error_code ec;

    // add a watch on the parent directory
    auto parentDir = mTimeZoneFilePath.parent_path();
    if (parentDir.empty())
    {
        AI_LOG_ERROR("time zone file '%s' does not have a parent directory, cannot monitor for changes",
                     mTimeZoneFilePath.c_str());
        return;
    }

    // create an inotify instance to monitor for any changes to the time zone file
    // in the parent directory
    int inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
    if (inotifyFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to initialize inotify");
        return;
    }

    // add a watch on the parent directory for any changes to the time zone file
    int dirWatchId = inotify_add_watch(inotifyFd, parentDir.c_str(),
                                       (IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_CLOSE_WRITE));
    if (dirWatchId < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to add inotify watch for directory '%s'", parentDir.c_str());
        close(inotifyFd);
        return;
    }

    // The initial target of the symlink, if it exists
    if (std::filesystem::is_regular_file(mTimeZoneFilePath, ec))
    {
        recheckTimeZoneFile();
    }
    else
    {
        if (ec)
        {
            AI_LOG_SYS_ERROR(ec.value(), "failed to check if '%s' is a regular file",
                             mTimeZoneFilePath.c_str());
        }
        else
        {
            AI_LOG_WARN("time zone file '%s' does not exist or is not a regular file, "
                        "waiting for it to appear", mTimeZoneFilePath.c_str());
        }
    }

    // Loop looking for events on the inotify file descriptor or the stop event fd
    while (true)
    {
        struct pollfd fds[2];
        fds[0].fd = mStopEventFd;
        fds[0].events = POLLIN;
        fds[0].revents = 0;
        fds[1].fd = inotifyFd;
        fds[1].events = POLLIN;
        fds[1].revents = 0;

        int result = TEMP_FAILURE_RETRY(poll(fds, 2, -1));
        if (result < 0)
        {
            AI_LOG_SYS_ERROR(errno, "poll failed");
            break;
        }

        if (fds[0].revents & POLLIN)
        {
            // stop event received, exit the loop
            break;
        }

        if (fds[1].revents & POLLIN)
        {
            processInotifyEvents(inotifyFd);
        }
    }

    if (close(inotifyFd) != 0)
        AI_LOG_SYS_ERROR(errno, "failed to close inotify file descriptor");

    AI_LOG_INFO("terminating time zone monitor loop for '%s'", mTimeZoneFilePath.c_str());
}

// -----------------------------------------------------------------------------
/**
 *  @brief Processes all the inotify events that are pending on the inotify
 *  file descriptor and checks
 *
 */
void TimeZoneMonitor::processInotifyEvents(int inotifyFd)
{
    uint8_t buffer[4096] __attribute__ ((aligned(__alignof__(struct inotify_event))));
    const struct inotify_event *event;

    while (true)
    {
        ssize_t length = TEMP_FAILURE_RETRY(read(inotifyFd, buffer, sizeof(buffer)));
        if ((length < 0) && (errno != EAGAIN))
            AI_LOG_SYS_ERROR(errno, "failed to read inotify events");

        if (length <= 0)
            break;

        const uint8_t *ptr = buffer;
        const uint8_t * const end = buffer + length;
        while (ptr < end)
        {
            event = reinterpret_cast<const struct inotify_event*>(ptr);
            if (event->len > 0)
            {
                const std::string_view eventName(event->name, event->len);
                if (mTimeZoneFileName == eventName)
                {
                    AI_LOG_DEBUG("received inotify event for time zone file '%s'", mTimeZoneFilePath.c_str());
                    recheckTimeZoneFile();
                }
            }

            ptr += (sizeof(struct inotify_event) + event->len);
        }
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Re-reads the time zone file and updates the registered paths if the
 *  target has changed.
 *
 *  This function is thread safe and can be called from the monitor loop when
 *  an event is received or from outside the monitor loop to force a re-check
 *  of the time zone file.
 */
void TimeZoneMonitor::recheckTimeZoneFile()
{
    std::error_code ec;

    // if the file doesn't exist then assume the time zone is not set and
    // wait for it to be created
    if (!std::filesystem::exists(mTimeZoneFilePath, ec))
        return;

    // expect the file to be a regular file, or a symlink to a regular file
    if (!std::filesystem::is_regular_file(mTimeZoneFilePath, ec))
    {
        AI_LOG_WARN("time zone file '%s' is not a regular file, cannot monitor for changes",
                    mTimeZoneFilePath.c_str());
        return;
    }

    // read the file contents
    int fd = open(mTimeZoneFilePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open time zone file '%s'", mTimeZoneFilePath.c_str());
        return;
    }

    std::vector<uint8_t> contents(MAX_TZ_FILE_SIZE);
    ssize_t rd = TEMP_FAILURE_RETRY(read(fd, contents.data(), contents.size()));
    if (rd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to read time zone file '%s'", mTimeZoneFilePath.c_str());
        close(fd);
        return;
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close time zone file '%s'", mTimeZoneFilePath.c_str());
    }

    if (rd == MAX_TZ_FILE_SIZE)
    {
        AI_LOG_WARN("time zone file '%s' is larger than expected, truncating",
                    mTimeZoneFilePath.c_str());
    }
    else
    {
        contents.resize(rd);
    }

    // take the lock and update the current time zone target and any registered paths
    std::lock_guard locker(mLock);

    if (contents != mTimeZoneFileContent)
    {
        AI_LOG_INFO("time zone file '%s' content changed, updating registered paths",
                    mTimeZoneFilePath.c_str());

        // update any registered paths to point to the new target
        for (const auto &path : mPathsToUpdate)
        {
            updateTimeZoneFile(path, contents);
        }

        // update the current time zone target
        mTimeZoneFileContent.swap(contents);
    }
}

// -----------------------------------------------------------------------------
/**
 *  @brief Updates a symlink to point to the new target of the time zone file.
 *
 *  Overwrites the existing symlink at \a linkPath to point to \a targetPath.
 */
void TimeZoneMonitor::updateTimeZoneFile(const std::filesystem::path &linkPath,
                                         const std::vector<uint8_t> &tzData)
{
    // create a random file extension to avoid conflicts with any existing temporary files
    static const char letters[] =
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

    std::random_device rd;
    std::mt19937 rng(rd());
    std::uniform_int_distribution<std::size_t> dist(0, sizeof(letters) - 1);

    std::string randomSuffix = ".tmp.";
    for (int i = 0; i < 6; i++)
        randomSuffix += letters[dist(rng)];

    // the temporary file path to write the new TZ data to
    auto tempPath = linkPath.string() + randomSuffix;

    // create a temporary file for the new TZ data
    int tempFd = open(tempPath.c_str(), O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
    if (tempFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create temporary file for time zone data");
        return;
    }

    // write the new TZ data to the temporary file
    ssize_t written = TEMP_FAILURE_RETRY(write(tempFd, tzData.data(), tzData.size()));
    if (written < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to write time zone data to temporary file");

        unlink(tempPath.c_str());
    }
    else if (static_cast<size_t>(written) != tzData.size())
    {
        AI_LOG_ERROR("failed to write all time zone data to temporary file, "
                     "expected %zu bytes but wrote %zd bytes",
                     tzData.size(), written);

        unlink(tempPath.c_str());
    }
    else
    {
        // atomically replace the old file with the new one
        if (rename(tempPath.c_str(), linkPath.c_str()) != 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to rename temporary symlink '%s' to '%s'",
                             tempPath.c_str(), linkPath.c_str());

            unlink(tempPath.c_str());
        }
        else
        {
            AI_LOG_INFO("updated time zone file at '%s'", linkPath.c_str());
        }
    }

    // close the temporary file, we don't need it anymore
    if (close(tempFd) != 0)
        AI_LOG_SYS_ERROR(errno, "failed to close temporary file for time zone data");
}
