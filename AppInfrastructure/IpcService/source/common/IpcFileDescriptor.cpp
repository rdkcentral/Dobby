/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2019 Sky UK
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
//
//  IpcFileDescriptor.cpp
//  IpcService
//
//

#include "IpcFileDescriptor.h"

#include <Logging.h>

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>


using namespace AI_IPC;

// -----------------------------------------------------------------------------
/*!
    \class IpcFileDescriptor
    \brief Light wrapper around a file descriptor so it can be used safely with
    dbus.

    Why do we need this?  Because we want to safely pass a file descriptor
    around using the DBusMessage class.

    Why not just use an integer? Because although it's obviously fine to pass
    an integer around, the life time of the file descriptor can get lost.  This
    class uses \c dup(2) to ensure that if the object was created with a valid
    file descriptor in the first place then it and all copy constructed objects
    will have a valid file descriptor.

 */


// -----------------------------------------------------------------------------
/*!
    Constructs a DBusFileDescriptor without a wrapped file descriptor. This is
    equivalent to constructing the object with an invalid file descriptor
    (like -1).

    \see fd() and isValid()
 */
IpcFileDescriptor::IpcFileDescriptor()
    : mFd(-1)
{
}

// -----------------------------------------------------------------------------
/*!
    Constructs a DBusFileDescriptor object by copying the fileDescriptor parameter.
    The original file descriptor is not touched and must be closed by the user.

    Note that the value returned by fd() will be different from the \a fd
    parameter passed.

    If the \a fd parameter is not valid, isValid() will return\c false and fd()
    will return \c -1.

    \see fd().
 */
IpcFileDescriptor::IpcFileDescriptor(int fd_)
    : mFd(-1)
{
    if (fd_ >= 0)
    {
        mFd = fcntl(fd_, F_DUPFD_CLOEXEC, 3);
        if (mFd < 0)
            AI_LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }
}

// -----------------------------------------------------------------------------
/*!
    Constructs a DBusFileDescriptor object by copying \a other.

 */
IpcFileDescriptor::IpcFileDescriptor(const IpcFileDescriptor &other)
    : mFd(-1)
{
    if (other.mFd >= 0)
    {
        mFd = fcntl(other.mFd, F_DUPFD_CLOEXEC, 3);
        if (mFd < 0)
            AI_LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }
}

// -----------------------------------------------------------------------------
/*!
    Move-assigns \a other to this DBusFileDescriptor.

 */
IpcFileDescriptor &IpcFileDescriptor::operator=(IpcFileDescriptor &&other)
{
    if ((mFd >= 0) && (::close(mFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close file descriptor");

    mFd = other.mFd;
    other.mFd = -1;

    return *this;
}

// -----------------------------------------------------------------------------
/*!
    Copies the Unix file descriptor from the \a other DBusFileDescriptor object.
    If the current object contained a file descriptor, it will be properly
    disposed of beforehand.

 */
IpcFileDescriptor &IpcFileDescriptor::operator=(const IpcFileDescriptor &other)
{
    if ((mFd >= 0) && (::close(mFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close file descriptor");

    mFd = -1;

    if (other.mFd >= 0)
    {
        mFd = fcntl(other.mFd, F_DUPFD_CLOEXEC, 3);
        if (mFd < 0)
            AI_LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }

    return *this;
}

// -----------------------------------------------------------------------------
/*!
    Destroys this DBusFileDescriptor object and disposes of the Unix file
    descriptor that it contained.

    \see reset() and clear()
 */
IpcFileDescriptor::~IpcFileDescriptor()
{
    reset();
}

// -----------------------------------------------------------------------------
/*!
    Returns \c true if this Unix file descriptor is valid. A valid Unix file
    descriptor is greater than or equal to 0.

    \see fd()
 */
bool IpcFileDescriptor::isValid() const
{
    return (mFd >= 0);
}

// -----------------------------------------------------------------------------
/*!
    Returns the Unix file descriptor contained by this DBusFileDescriptor object.
    An invalid file descriptor is represented by the value -1.

    Note that the file descriptor returned by this function is owned by the
    IpcFileDescriptor object and must not be stored past the lifetime of this
    object. It is ok to use it while this object is valid, but if one wants to
    store it for longer use you should use the IpcFileDescriptor::dup() function.

    \see isValid(), dup()
 */
int IpcFileDescriptor::fd() const
{
    return mFd;
}

// -----------------------------------------------------------------------------
/*!
    Returns a dup'd copy of the file descriptor.  The caller is responsible for
    closing the file descriptor when it is no longer required.

    This function sets the O_CLOEXEC flag on the returned file descriptor.

    If the file descriptor stored by the object is invalid (ie. isValid()
    returns \c false) then -1 will be returned.

    \see fd()
 */
int IpcFileDescriptor::IpcFileDescriptor::dup() const
{
    return fcntl(mFd, F_DUPFD_CLOEXEC, 3);
}

// -----------------------------------------------------------------------------
/*!
    Closes the file descriptor contained by this DBusFileDescriptor object.
    And dup's a copy of the supplied \a fd if not -1.

    \see clear()
 */
void IpcFileDescriptor::reset(int fd_)
{
    if ((mFd >= 0) && (::close(mFd) != 0))
        AI_LOG_SYS_ERROR(errno, "failed to close file descriptor");

    mFd = -1;

    if (fd_ >= 0)
    {
        mFd = fcntl(fd_, F_DUPFD_CLOEXEC, 3);
        if (mFd < 0)
            AI_LOG_SYS_ERROR(errno, "failed to dup supplied fd");
    }
}

// -----------------------------------------------------------------------------
/*!
    Same as reset, added to match c++11 naming convention.

    \see reset()
 */
void IpcFileDescriptor::clear()
{
    reset();
}

// -----------------------------------------------------------------------------
/*!
    FIXME


 */
bool IpcFileDescriptor::operator==(const IpcFileDescriptor &rhs) const
{
    return mFd == rhs.mFd;
}