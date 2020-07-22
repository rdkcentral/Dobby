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
 * File:   IDobbyUtils.h
 *
 */
#ifndef IDOBBYUTILS_H
#define IDOBBYUTILS_H

#include <ContainerId.h>

#include <cstdint>
#include <string>
#include <list>
#include <chrono>
#include <functional>
#include <memory>
#include <map>
#include <string.h>

#include <sys/types.h>
#include <sys/sysmacros.h>


// -----------------------------------------------------------------------------
/**
 *  @class IDobbyUtils_v1
 *  @brief Interface that exports some utilities that plugins may find useful
 *
 *  As it's name implies this is just a collection of standalone utility
 *  functions that wrap up some of the common things that plugins do.
 *
 *
 */
class IDobbyUtils_v1
{
public:
    virtual ~IDobbyUtils_v1() = default;

public:

    // -------------------------------------------------------------------------
    /**
     *  @brief Makes a directory and all parent directories as needed
     *
     *  This is equivalent to the 'mkdir -p <dir>' command.
     *
     *  All directories created will have access mode set by @a mode, for this
     *  reason the mode should be at least 'rwx------'.
     *
     *  If the pathname given in pathname is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor dirFd, if
     *  dirFd is not supplied then it's relative to the cwd.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to the directory to create.
     *  @param[in]  mode            The file access mode to give to all
     *                              directories created.
     *
     *  @return true on success, false on failure.
     */
    virtual bool mkdirRecursive(const std::string& path, mode_t mode) const = 0;
    virtual bool mkdirRecursive(int dirFd, const std::string& path, mode_t mode) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Removes a directory and all it's contents.
     *
     *  This is equivalent to the 'rm -rf <dir>' command.
     *
     *  If the pathname given in pathname is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor dirFd, if
     *  dirFd is not supplied then it's relative to the cwd.
     *
     *  @warning This function only supports deleting directories with contents
     *  that are less than 128 levels deep, this is to avoid running out of
     *  file descriptors.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to the directory to create.
     *  @param[in]  mode            The file access mode to give to all
     *                              directories created.
     *
     *  @return true on success, false on failure.
     */
    virtual bool rmdirRecursive(const std::string& path) const = 0;
    virtual bool rmdirRecursive(int dirFd, const std::string& path) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Removes the contents of a directory but leave the actual
     *  directory in place.
     *
     *  This is equivalent to the 'rm -rf <dir>/ *' command.
     *
     *  If the pathname given in pathname is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor dirFd, if
     *  dirFd is not supplied then it's relative to the cwd.
     *
     *  @warning This function only supports deleting directories with contents
     *  that are less than 128 levels deep, this is to avoid running out of
     *  file descriptors.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to the directory to create.
     *  @param[in]  mode            The file access mode to give to all
     *                              directories created.
     *
     *  @return true on success, false on failure.
     */
    virtual bool rmdirContents(const std::string& path) const = 0;
    virtual bool rmdirContents(int dirFd, const std::string& path) const = 0;
    virtual bool rmdirContents(int dirFd) const = 0;


    // -------------------------------------------------------------------------
    /**
     *  @brief Associates a give file descriptor with a loop device
     *
     *  First the function attempts to get a free loop device, if that succeeds
     *  it attaches the supplied file descriptor to it and returns an fd to
     *  the loop device and (optionally) writes the path to the loop device
     *  in the @a loopDevPath string.
     *
     *  @param[in]  fileFd          An open file descriptor to associate with
     *                              the loop device.
     *  @param[out] loopDevPath     If not null, the method will write the path
     *                              to the loop device dev node into the string
     *
     *  @return on success returns the open file descriptor to the loop device
     *  associated with the file, on failure -1.
     */
    virtual int loopDeviceAssociate(int fileFd, std::string* loopDevPath = nullptr) const = 0;


    // -------------------------------------------------------------------------
    /**
     *  @brief Runs the e2fsck tool on a file system image to check it's integrity
     *
     *  This function does a fork/exec to launch the process, it drops root
     *  privilages and runs the tool as user 1000:1000, therefore the file
     *  that is being checked should be readable and writeble by user 1000.
     *
     *  If this function returns false the image file should probably be
     *  deleted / reformatted.
     *
     *  @param[in]  dirFd           The fd of the directory containing the
     *                              the image to check.  The function will
     *                              switch to this directory before dropping
     *                              permissions (provided it's not AT_FCWD).
     *  @param[in]  imageFileName   The name of the file to check.
     *  @param[in]  repair          If true we'll ask the tool to try and repair
     *                              the file if it detects any errors.
     *
     *  @return if the file passes the check (or was successifully repaired)
     *  true is returned, otherwise false.
     */
    virtual bool checkExtImageFile(int dirFd, const std::string& imageFileName,
                                   bool repair = true) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Runs the mke2fs tool to format a file system image
     *
     *  This function does a fork/exec to launch the process, it drops root
     *  privilages and runs the tool as user 1000:1000, therefore the file
     *  that it's formatting should be readable and writeble by user 1000.
     *
     *
     *  @param[in]  dirFd           The fd of the directory containing the
     *                              the image to write.  The function will
     *                              switch to this directory before dropping
     *                              permissions (provided it's not AT_FCWD).
     *  @param[in]  imageFileName   The name of the file to format, it must
     *                              already exist.
     *  @param[in]  fsType          The ext version to format the file as,
     *                              this is equivalent to the '-t' option and
     *                              should be one of; 'ext2', 'ext3' or 'ext4'
     *
     *  @return on success returns true on failure false.
     */
    virtual bool formatExtImageFile(int dirFd, const std::string& imageFileName,
                                    const std::string& fsType = "ext4") const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Logs and deletes any files found in the lost+found directory of
     *  the mount point.
     *
     *  This was added for NGDEV-133724; we should be clearing the lost+found to
     *  avoid cruft building up and taking all the space in the loop mount.
     *
     *
     *  @param[in]  mountPoint      The absolute path to the mounted device,
     *                              NOT the the location of the lost+found dir.
     *  @param[in]  logTag          If not empty then a log warning message will
     *                              be printed containing the name of the file
     *                              that was deleted and referencing the the
     *                              string in logTag.
     */
    virtual void cleanMountLostAndFound(const std::string& mountPoint,
                                        const std::string& logTag = std::string()) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Simply writes a string into a file
     *
     *  Not much more to say really.
     *
     *  If the pathname given in @a filePath is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor @a dirFd,
     *  if @a dirFd is not supplied then it's relative to the cwd.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to file to write to.
     *  @param[in]  flags           Open flags, these will be OR'd with O_WRONLY
     *                              and O_CLOEXEC.
     *  @param[in]  mode            The file access mode to set if O_CREAT was
     *                              specified in flags and the file was created.
     *
     *  @return true on success, false on failure.
     */
    virtual bool writeTextFile(const std::string& path,
                               const std::string& str,
                               int flags, mode_t mode = 0644) const = 0;
    virtual bool writeTextFileAt(int dirFd, const std::string& path,
                                 const std::string& str,
                                 int flags, mode_t mode = 0644) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Simply read a string from a file
     *
     *  Not much more to say really.
     *
     *  If the pathname given in @a filePath is relative, then it is interpreted
     *  relative to the directory referred to by the file descriptor @a dirFd,
     *  if @a dirFd is not supplied then it's relative to the cwd.
     *
     *  @param[in]  dirFd           If specified the path should be relative to
     *                              to this directory.
     *  @param[in]  path            The path to file to write to.
     *  @param[in]  maxLen          The maxiumum number of characters to read,
     *                              defaults to 4096.
     *
     *  @return the string read from the file, on failure an empty string.
     */
    virtual std::string readTextFile(const std::string& path,
                                     size_t maxLen = 4096) const = 0;
    virtual std::string readTextFileAt(int dirFd, const std::string& path,
                                       size_t maxLen = 4096) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns a file descriptor to the given namespace of the process
     *
     *  The caller is responsible for closing the returned file descriptor when
     *  it is no longer required.
     *
     *  The returned namespace can used in the setns(...) call, or other calls
     *  that enter / manipulate namespaces.
     *
     *  @param[in]  pid         The pid of the process to get the namespace of.
     *  @param[in]  nsType      The type of namespace to get, it should be one
     *                          of the CLONE_NEWxxx constants, see the setns
     *                          man page for possible values.
     *
     *  @return on success the file descriptor to the given namespace, on
     *  failure -1
     */
    virtual int getNamespaceFd(pid_t pid, int nsType) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Calls the given function in the namespace of given pid
     *
     *  You'd typically use this to perform options in the namespace of a
     *  container.  The @a pid argument would be the pid of the containered
     *  processes as passed in one of the pre/post hook functions.
     *
     *  The @a nsType argument should be one of the following values:
     *      CLONE_NEWIPC  - run in a IPC namespace
     *      CLONE_NEWNET  - run in a network namespace
     *      CLONE_NEWNS   - run in a mount namespace
     *      CLONE_NEWPID  - run in a  PID namespace
     *      CLONE_NEWUSER - run in a user namespace
     *      CLONE_NEWUTS  - run in a UTS namespace
     *
     *
     *  The following is an example of how you could create a listening socket
     *  within a container
     *
     *  @code
     *
     *      TODO:
     *
     *  @endcode
     *
     *  @param[in]  pid         The pid owner of the namespace to enter,
     *                          typically the pid of the process in the
     *                          container.
     *  @param[in]  nsType      The type of the namespace to enter, see above.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    template< class Function >
    inline bool callInNamespace(pid_t pid, int nsType, Function func) const
    {
        return this->callInNamespaceImpl(pid, nsType, func);
    }

    // -------------------------------------------------------------------------
    /**
     *  @brief Slightly nicer version of callInNamespace, handles the function
     *  bind for you automatically
     *
     *  See above version for details on the function.
     *
     *  @param[in]  pid         The pid owner of the namespace to enter,
     *                          typically the pid of the process in the
     *                          container.
     *  @param[in]  nsType      The type of the namespace to enter, see above.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    template< class Function, class... Args >
    inline bool callInNamespace(pid_t pid, int nsType, Function&& f, Args&&... args) const
    {
        return this->callInNamespaceImpl(pid, nsType, std::bind(std::forward<Function>(f),
                                                                std::forward<Args>(args)...));
    }


    // -------------------------------------------------------------------------
    /**
     *  @brief Adds a new timer to the timer queue
     *
     *  The @a handler function will be called after the timeout period and then
     *  if @a oneShot is false periodically at the given timeout interval.
     *
     *  The @a handler will be called from the context of the timer queue, bare
     *  in mind for any locking restrictions.
     *
     *  A timer can be cancelled by either calling @a cancelTimer() or returning
     *  false from the handler.  One shot timers are automatically removed after
     *  they are fired, there is not need to call @a cancelTimer() for them.
     *
     *  @param[in]  timeout     The time after which to call the supplied
     *                          handler.
     *  @param[in]  oneShot     If true the timer is automatically removed after
     *                          it times out the first time.
     *  @param[in]  handler     The handler function to call when the timer
     *                          times out.
     *
     *  @return on success returns a (greater than zero) timer id integer which
     *  identifies the timer, on failure -1 is returned.
     */
    template< class Rep, class Period >
    inline int startTimer(const std::chrono::duration<Rep, Period>& timeout,
                          bool oneShot,
                          const std::function<bool()>& handler) const
    {
        return this->startTimerImpl(std::chrono::duration_cast<std::chrono::milliseconds>(timeout),
                                    oneShot,
                                    handler);
    }

    // -------------------------------------------------------------------------
    /**
     *  @brief Removes the given timer from the timer queue
     *
     *  Once this method returns (successfully) you are guaranteed that the
     *  timer handler will not be called, i.e. this is synchronisation point.
     *
     *  This method will fail if called from the context of a timer handler, if
     *  you want to cancel a repeating timer then just return false in the
     *  handler.
     *
     *  @param[in]  timerId     The id of the timer to cancel as returned by the
     *                          startTimer() method.
     *
     *  @return true if the timer was found and was removed from the queue,
     *  otherwise false
     */
    virtual bool cancelTimer(int timerId) const = 0;


    // -------------------------------------------------------------------------
    /**
     *  @brief Call the given function in the namespace of the descriptor
     *
     *  To get a namespace descriptor of a given process you just need to open
     *  one of the files in /proc/<pid>/ns/
     *
     *  @param[in]  namespaceFd The namespace file descriptor to run the
     *                          function in.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    template< class Function >
    inline bool callInNamespace(int namespaceFd, Function func) const
    {
        return this->callInNamespaceImpl(namespaceFd, func);
    }

    template< class Function, class... Args >
    inline bool callInNamespace(int namespaceFd, Function&& f, Args&&... args) const
    {
        return this->callInNamespaceImpl(namespaceFd, std::bind(std::forward<Function>(f),
                                                                std::forward<Args>(args)...));
    }


    // -------------------------------------------------------------------------
    /**
     *  @brief Returns the major number assigned to a given driver
     *
     *  This function tries to find the major number assigned to a given driver,
     *  it does this by parsing the /proc/devices file.
     *
     *  @warning Currently this function doesn't work for 'misc' devices, which
     *  are devices listed by /proc/misc.
     *
     *  @param[in]  driverName  The name of the driver to get the major number
     *                          for.
     *
     *  @return if found the major number is returned, if not found then 0 is
     *  returned.
     */
    virtual unsigned int getDriverMajorNumber(const std::string &driverName) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Returns true if the given device is allowed in the container
     *
     *  This is here for security reasons as I didn't want just any device added
     *  to the container whitelist.  If we trust the code on the other end of
     *  Dobby that is creating the containers then this is not needed, but just
     *  in case that got hacked I didn't want people to create containers
     *  enabling access to CDI / system device nodes.
     *
     *  @warning This method doesn't take into account drivers being insmod /
     *  rmmod and the re-use of major numbers, however if a user could do that
     *  then this check is the least of our problems.
     *
     *  @param[in]  major       The major number of the device.
     *  @param[in]  minor       The minor number of the device.
     *
     *  @return true if the device is allowed, otherwise false.
     */
    virtual bool deviceAllowed(dev_t device) const = 0;

    inline bool deviceAllowed(unsigned int major, unsigned int minor) const
    {
        return this->deviceAllowed(makedev(major, minor));
    }

protected:

    // -------------------------------------------------------------------------
    /**
     *  @brief Implementation of the callInNamespace public interface
     *
     *  @see IDobbyUtils::callInNamespace
     *
     *  @param[in]  pid         The pid owner of the namespace to enter,
     *                          typically the pid of the process in the
     *                          container.
     *  @param[in]  nsType      The type of the namespace to enter, see above.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    virtual bool callInNamespaceImpl(pid_t pid, int nsType, const std::function<void()>& func) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Implementation of the callInNamespace public interface
     *
     *  @see IDobbyUtils::callInNamespace
     *
     *  @param[in]  namespaceFd The namespace file descriptor to run the
     *                          function in.
     *  @param[in]  func        The actual function to execute.
     *
     *  @return true on success, false on failure.
     */
    virtual bool callInNamespaceImpl(int namespaceFd, const std::function<void()>& func) const = 0;

    // -------------------------------------------------------------------------
    /**
     *  @brief Adds a new timer to the timer queue
     *
     *  @see IDobbyUtils::startTimer
     *
     *  @param[in]  timeout     The time after which to call the supplied
     *                          handler.
     *  @param[in]  oneShot     If true the timer is automatically removed after
     *                          it times out the first time.
     *  @param[in]  handler     The handler function to call when the timer
     *                          times out.
     *
     *  @return on success returns a (greater than zero) timer id integer which
     *  identifies the timer, on failure -1 is returned.
     */
    virtual int startTimerImpl(const std::chrono::milliseconds& timeout,
                               bool oneShot,
                               const std::function<bool()>& handler) const = 0;


};


// -----------------------------------------------------------------------------
/**
 *  @class IDobbyUtils_v2
 *  @brief Second version of the interface containing extra functions for
 *  working with iptables.
 *
 *
 *
 */
class IDobbyUtils_v2 : public virtual IDobbyUtils_v1
{
public:
    using IDobbyUtils_v1::mkdirRecursive;
    using IDobbyUtils_v1::rmdirRecursive;
    using IDobbyUtils_v1::rmdirContents;
    using IDobbyUtils_v1::loopDeviceAssociate;
    using IDobbyUtils_v1::checkExtImageFile;
    using IDobbyUtils_v1::formatExtImageFile;
    using IDobbyUtils_v1::cleanMountLostAndFound;
    using IDobbyUtils_v1::writeTextFile;
    using IDobbyUtils_v1::writeTextFileAt;
    using IDobbyUtils_v1::readTextFile;
    using IDobbyUtils_v1::readTextFileAt;
    using IDobbyUtils_v1::getNamespaceFd;
    using IDobbyUtils_v1::cancelTimer;
    using IDobbyUtils_v1::getDriverMajorNumber;
    using IDobbyUtils_v1::deviceAllowed;

public:

    // -------------------------------------------------------------------------
    /**
     *  @brief Sets / Gets integer meta data for the given container.
     *
     *  You can use this to share meta data about the container across different
     *  plugins.  For example if network namespaces are enabled.
     *
     *  The data is cleared automatically when the container is shutdown.
     */
    virtual void setIntegerMetaData(const ContainerId &id, const std::string &key, int value) = 0;
    virtual int getIntegerMetaData(const ContainerId &id, const std::string &key, int defaultValue) const = 0;
    inline int getIntegerMetaData(const ContainerId &id, const std::string &key)
    {
        return getIntegerMetaData(id, key, -1);
    }

    // -------------------------------------------------------------------------
    /**
     *  @brief Sets / Gets string meta data for the given container.
     *
     *  You can use this to share meta data about the container across different
     *  plugins.  For example the ip address assigned to the container.
     *
     *  The data is cleared automatically when the container is shutdown.
     */
    virtual void setStringMetaData(const ContainerId &id, const std::string &key, const std::string &value) = 0;
    virtual std::string getStringMetaData(const ContainerId &id, const std::string &key, const std::string &defaultValue) const = 0;
    inline std::string getStringMetaData(const ContainerId &id, const std::string &key)
    {
        return getStringMetaData(id, key, std::string());
    }
    virtual void clearContainerMetaData(const ContainerId &id) = 0;

};


using IDobbyUtils = IDobbyUtils_v2;



#endif // !defined(IDOBBYUTILS_H)
