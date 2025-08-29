/*
* If not stated otherwise in this file or this component's LICENSE file the
* following copyright and licenses apply:
*
* Copyright 2014 Sky UK
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
 * File:   FileUtilities.cpp
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 11 July 2014
 *
 */

#include "FileUtilities.h"

#include <Logging.h>

#if defined(ANDROID)
#  include <openssl/md5.h>
#else
#  include <AI_MD5.h>
#endif

#include <unistd.h>
#include <stddef.h>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/xattr.h>
#include <sys/statvfs.h>
#include <fcntl.h>
#include <iostream>
#include <fstream>
#include <errno.h>
#include <dirent.h>
#include <ftw.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include <cstdlib>
#include <limits.h>
#include <fstream>
#include <list>

using namespace std;

namespace
{
    thread_local size_t gTotalBytes{0};  // thread local makes this thread safe
    int GetSize( const char *path, const struct stat *statPtr, int currentFlag, struct FTW *internalFtwUsage )
    {
        if ( FTW_F == currentFlag ) // ignore everything else
        {
            struct stat buf;
            if (stat(path, &buf) == 0)
            {
                // Check for sparse file
                if(buf.st_size > (buf.st_blocks * buf.st_blksize))
                {
                    return 0;  //ntfw continues
                }
            }

            gTotalBytes += statPtr->st_size;
        }
        return 0;  //ntfw continues
    }
} // namespace

size_t AICommon::getDirectorySizeInKb( const std::string &path )
{
    const int flags                   = FTW_DEPTH | FTW_MOUNT | FTW_PHYS;
    const int maxFileDescriptorsToUse = 10; // or whatever
    const int result                  = nftw( path.c_str(), GetSize, maxFileDescriptorsToUse, flags );
    size_t tmpResult = 0;
    if ( result == -1 )
    {
        AI_LOG_ERROR( "nftw returned with [%d] ", result );
    }

    tmpResult = (gTotalBytes/1024);

    // Reset the global
    gTotalBytes = 0;

    return tmpResult;
}

std::vector<std::string> AICommon::splitPath(const std::string& path)
{
    std::stringstream ss(path);
    std::vector<std::string> directories;
    std::string token;
    while(getline(ss, token, '/'))
    {
        if(!token.empty())
        {
            directories.emplace_back(std::move(token));
        }
    }
    return directories;
}

bool AICommon::mkdirRecursive(const std::string& path, mode_t mode /*= S_IRWXU*/)
{
    auto directories = AICommon::splitPath(path);
    if(!directories.empty())
    {
        //start with / if it's an absolute path
        std::string partial = path[0] == '/' ? "/" : "";
        for(size_t i = 0; i < directories.size(); ++i)
        {
            bool created = true;

            partial += directories[i] + "/";
            if(mkdir(partial.c_str(), mode) != 0)
            {
                //if the directory already exists errno is EEXIST. We're ignoring that.
                //if there's file where we want to create directory, errno is set to ENOTDIR.
                if(errno == EEXIST)
                {
                    created = false;
                }
                else
                {
                    return false;
                }
            }

            //fusion sets a very restrictive umask, meaning we have to force our
            //mode flags to stop them getting wiped out.
            if (created && (chmod(partial.c_str(), mode) != 0))
            {
                return false;
            }
        }
    }

    return true;
}

bool AICommon::mkdirRecursiveAt(const std::string& fullPath, int parentDirectoryFd, mode_t mode /*= S_IRWXU*/)
{
    // this function treats absolute paths as paths within parentDirectoryFd
    const std::vector<std::string> dirParts = AICommon::splitPath(fullPath);

    std::string path;
    for (const std::string & part : dirParts)
    {
        if(!part.empty())
        {
            bool created = true;

            path += part + '/';
            if(0 != mkdirat(parentDirectoryFd, path.c_str(), mode))
            {
                if(errno == EEXIST)
                {
                    created = false;
                }
                else
                {
                    return false;
                }
            }

            // once again fusion imposes umask restrictions on dirs, we need
            // to override here
            if(created && (0 != fchmodat(parentDirectoryFd, path.c_str(), mode, 0)))
            {
                return false;
            }
        }
    }

    return true;
}


bool AICommon::exists(const std::string& path)
{
    struct stat buffer;
    return stat (path.c_str(), &buffer) == 0;
}

/*
 * delete_all and delete_directory
 *
 * The directory that has been created needs to be removed.
 * So these functions are used to remove the directory that has
 * just been created.
 */
static int delete_all(const char *fpath, const struct stat *, int tflag, struct FTW *)
{
    switch (tflag)
    {
    case FTW_DP:
	if (rmdir(fpath) != 0)  
	{
		perror("rmdir failed");  //Error message printed
		return 1;                //Error propagated
	}
	break;

    case FTW_F:     // file
    case FTW_SL:    // un-followed sym-link
        unlink(fpath);
        break;

    default:
        AI_LOG_ERROR("Un-expected file type found in directory %s type(%d) exiting.", fpath, tflag);

        // rage quit - not handled file type
        return 1;
    };

    return 0;
}

void AICommon::deleteDirectory(const std::string &directoryName)
{
    AI_LOG_FN_ENTRY();

    // use the unix walk to walk all the files and call the delete
    // function on all of them. It should walk the depth first so
    // that the directories can be deleted.
    if (nftw(directoryName.c_str(), delete_all, 20, (FTW_PHYS | FTW_DEPTH)) == -1)
    {
        AI_LOG_WARN("failed to delete %s", directoryName.c_str());
    }

    AI_LOG_FN_EXIT();
}

bool AICommon::deleteFilesInDirectory( const std::string &directoryName )
{
    AI_LOG_FN_ENTRY();
    bool result = false;

    // use the unix walk to walk all the files and call the delete
    // function on all of them.
    DIR *dirName = opendir( directoryName.c_str() );
    if ( dirName )
    {
        std::list<pair < unsigned, string> > toDelete;

        struct dirent64 *appDirent;
        while ( ( appDirent = readdir64( dirName ) ) != NULL )
        {
            // skip the . and .. links
            if ( ( appDirent->d_name[0] == '.' ) && ( ( appDirent->d_name[1] == '\0' ) ||
                                                      ( ( appDirent->d_name[1] == '.' ) &&
                                                        ( appDirent->d_name[2] == '\0' ) ) ) )
            {
                continue;
            }

            // delete anything that's not a directory
            if ( appDirent->d_type != DT_DIR )
            {
                toDelete.push_back( std::make_pair<unsigned, string>( appDirent->d_type, appDirent->d_name ) );
                continue;
            }

        }

        std::list<pair < unsigned, string> > ::const_iterator
        it = toDelete.begin();
        for ( ; it != toDelete.end(); ++it )
        {
            if ( AICommon::deleteFile( directoryName + "/" + it->second ) )
            {
                result = true;

            }
            else
            {

                AI_LOG_ERROR( "Could not delete [%s]", it->second.c_str() );

                result = false;
            }
        }
        closedir( dirName );
    }

    AI_LOG_FN_EXIT();

    return result;
}

void AICommon::deleteDirectoryAt(int parentDirectoryFd, const std::string& directoryName)
{
    AI_LOG_FN_ENTRY();

    // sanity check
    if (directoryName.empty())
    {
        AI_LOG_ERROR("invalid directory name");
        return;
    }

    // construct the path relative to parentDirectoryFd
    std::string dirPath;
    if ((parentDirectoryFd == AT_FDCWD) || (directoryName.front() == '/'))
    {
        dirPath = directoryName;
    }
    else
    {
        char basePath[64];
        sprintf(basePath, "/proc/self/fd/%d/", parentDirectoryFd);
        dirPath = basePath;
        dirPath += directoryName;
    }

    // use the unix walk to walk all the files and call the delete
    // function on all of them. It should walk the depth first so
    // that the directories can be deleted.
    if (nftw(dirPath.c_str(), delete_all, 20, (FTW_PHYS | FTW_DEPTH)) == -1)
    {
        AI_LOG_SYS_WARN(errno, "failed to delete %s", directoryName.c_str());
    }

    AI_LOG_FN_EXIT();
}

bool AICommon::deleteFile(const std::string & filePath)
{
    return unlink(filePath.c_str()) == 0;
}

// TODO:Possibly(?) allow file perms to be passed in
int AICommon::copyFile(const std::string &to, const std::string &from)
{
    int fdTo = -1, fdFrom = -1;
    char buf[4096];
    ssize_t nread;
    int saved_errno;

    fdFrom = open(from.c_str(), O_RDONLY | O_CLOEXEC);
    if (fdFrom < 0)
    {
        return -1;
    }

    fdTo = open(to.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fdTo < 0)
    {
        goto out_error;
    }

    while ((nread = TEMP_FAILURE_RETRY(read(fdFrom, buf, sizeof(buf)))) > 0)
    {
        char *out_ptr = buf;
        size_t remaining = static_cast<size_t>(nread);

        while (remaining > 0)
        {
            ssize_t nwritten = TEMP_FAILURE_RETRY(write(fdTo, out_ptr, remaining));
            if (nwritten >= 0)
            {
                if (static_cast<size_t>(nwritten) > remaining)
                {
                    // Should never happen, but guard against overflow
                    goto out_error;
                }

                remaining -= static_cast<size_t>(nwritten);
                out_ptr += nwritten;
            }
            else if (errno != EINTR)
            {
                goto out_error;
            }
        }
    }

    if (nread == 0)
    {
        if (close(fdTo) < 0)
        {
            fdTo = -1;
            goto out_error;
        }
        close(fdFrom);
        return 0;
    }

out_error:
    saved_errno = errno;
    if (fdFrom != -1)
        close(fdFrom);
    if (fdTo != -1)
        close(fdTo);
    errno = saved_errno;
    return -1;
}

static std::vector<std::string> getFilesInDirectory(int dirFd, const std::string &prefix)
{
    std::vector<std::string> out;

    DIR *dir = fdopendir(dirFd);
    if (!dir)
    {
        AI_LOG_SYS_ERROR(errno, "Failed top open directory");
        close(dirFd);
        return out;
    }

    struct dirent *entry = nullptr;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_REG)
        {
            if (!prefix.empty())
                out.push_back(prefix + entry->d_name);
            else
                out.push_back(entry->d_name);
        }
    }

    closedir(dir);

    return out;
}

std::vector<std::string> AICommon::getFilesInDirectory(int dirFd)
{
    int fd = openat(dirFd, ".", O_CLOEXEC | O_DIRECTORY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to open directory fd");
        return std::vector<std::string>();
    }

    return ::getFilesInDirectory(fd, std::string());
}

std::vector<std::string> AICommon::getFilesInDirectory(const std::string & dirName, bool fullPaths)
{
    std::vector<std::string> files;

    int dirFd = open(dirName.c_str(), O_CLOEXEC | O_DIRECTORY);
    if (dirFd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to open directory @ '%s'", dirName.c_str());
        return files;
    }

    std::string prefix;
    if (fullPaths)
    {
        prefix = dirName + "/";
    }

    return ::getFilesInDirectory(dirFd, prefix);
}

std::string AICommon::resolvePath(const std::string& in)
{
    AI_LOG_FN_ENTRY();

    std::string out;
    char buf[PATH_MAX];
    if(realpath(in.c_str(), buf) == buf)
    {
        out = std::string(buf);
    }
    else
    {
        AI_LOG_SYS_WARN(errno, "failed to resolve %s", in.c_str());
        throw std::invalid_argument("Cannot resolve path " + in);
    }

    AI_LOG_FN_EXIT();
    return out;
}

std::string AICommon::readTextStream(FILE *fp)
{
    char buf[4*1024];
    std::string result;

    while(fgets(buf, sizeof(buf)-1, fp))
    {
        result.append(buf);
    }

    if (ferror(fp))
    {
        throw std::runtime_error("Error whilst reading from CryptoTool stdout pipe");
    }

    return result;
}

std::vector<uint8_t> AICommon::fileContents(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::in|std::ios::binary|std::ios::ate);
    std::vector<uint8_t> out;
    const std::ios::pos_type fileSize = file.tellg();
    if(-1 != fileSize)
    {
        out.resize(fileSize);
        file.seekg(0, std::ios::beg);
        file.read((char*)out.data(), out.size());
        file.close();
    }

    return out;
}

std::vector<uint8_t> AICommon::fileContents(int fd, size_t maxSize)
{
    // copy the entire contents into a string
    std::vector<uint8_t> contents;
    contents.reserve(1024);

    ssize_t n;
    uint8_t buf[512];

    while ((n = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)))) > 0)
    {
        // append the data
        contents.insert(contents.end(), buf, buf + n);

        // sanity check the size of the config.xml file
        if (contents.size() > maxSize)
        {
            n = -1;
            errno = ENOMEM;
            break;
        }
    }

    // check for errors
    if (n < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to read entire config.xml file");
        contents.clear();
    }

    return contents;
}

std::vector<uint8_t> AICommon::fileContents(int dirFd, const std::string& filepath, size_t maxSize)
{
    // open the file
    int fd = openat(dirFd, filepath.c_str(), O_CLOEXEC | O_RDONLY);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to open '%s' file", filepath.c_str());
        return std::vector<uint8_t>();
    }

    // read the file
    const std::vector<uint8_t> contents = fileContents(fd, maxSize);

    // close the file
    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to close '%s' file", filepath.c_str());
    }

    return std::move(contents);
}

bool AICommon::compareFilesExactly(const std::string & filePathApples, const std::string & filePathOranges)
{
    std::ifstream streamApples;
    streamApples.open(filePathApples, std::ios_base::in | std::ios_base::binary);
    if (streamApples.is_open())
    {
        std::ifstream streamOranges;
        streamOranges.open(filePathOranges, std::ios_base::in | std::ios_base::binary);
        if (streamOranges.is_open())
        {
            char bufferApples[1024];
            char bufferOranges[1024];

            streamApples.read(bufferApples, sizeof(bufferApples));
            streamOranges.read(bufferOranges, sizeof(bufferOranges));

            if (streamApples.gcount() == streamOranges.gcount())
            {
                int match = streamApples.gcount();
                for (int c = 0; c < match; ++c)
                {
                    if (bufferApples[c] != bufferOranges[c]) return false;
                }
            }
            else
            {
                return false;
            }

            if (streamApples.eof() != streamOranges.eof())
            {
                return false;
            }

            if (streamApples.eof() && streamOranges.eof())
            {
                return true;
            }
        }
    }

    return false;
}


bool AICommon::createTextFileAt(int dirFd, const std::string& filePath, const std::string& contents, mode_t mode /*= S_IRWXU*/)
{
    int fd = openat(dirFd, filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "failed to create '%s'", filePath.c_str());
        return false;
    }

    const char* dataPtr = contents.data();
    size_t remaining = contents.size();

    while (remaining > 0)
    {
        ssize_t ret = TEMP_FAILURE_RETRY(write(fd, dataPtr, remaining));
        if (ret < 0)
        {
            AI_LOG_SYS_ERROR(errno, "failed to write %zu bytes to '%s' file", remaining, filePath.c_str());
            break;
        }
        else if (ret == 0)
        {
            AI_LOG_ERROR("didn't write any data, odd");
            break;
        }
        else
        {
            if (static_cast<size_t>(ret) > remaining)
            {
                AI_LOG_SYS_ERROR(errno, "write returned more bytes than expected");
                break;
            }

            remaining -= static_cast<size_t>(ret);
            dataPtr += ret;
        }
    }

     // because of the restrictive umask set in fusion; setting the mode in the above open
    // call is not enough, so do an explicit chmod here to enforce the perms
    if (fchmod(fd, mode) < 0)
    {
        AI_LOG_SYS_WARN(errno, "failed to set mode on file to 0%03o", mode);
    }

    if (close(fd) != 0)
    {
        AI_LOG_SYS_WARN(errno, "failed to close file '%s'", filePath.c_str());
    }

    return (remaining == 0);
}

bool AICommon::createTextFile(const std::string& filePath, const std::string& contents, mode_t mode /*= S_IRWXU*/)
{
    return createTextFileAt(AT_FDCWD, filePath, contents, mode);
}

/**
 * @brief Returns the value of the xattr with the given key
 *
 * This function can fail for various reasons, in which case the return
 * value is false and errno is set to indicate the error. See the fgetxattr
 * man page for a list of errors.
 *
 * The length of the value is limited to 4096 bytes, which is the typical max
 * value for an xattr on an EXT file system.
 *
 *
 * @param[in] fileFd   The descriptor of the file or directory that
 *                     contains the xattr
 * @param[in] key      The key of the xattr
 *
 * @return On failure a boost::none will be returned, on success the optional
 * will contain the string value, an empty attribute is not classed as an
 * error
 */
boost::optional<std::string> AICommon::getXAttrib(int fileFd, const std::string& key)
{
    ssize_t bufLen = 128;
    char* buf = new char[bufLen];

    // get the xattr with initial sized buffer, this will return
    // the actual size of the buffer or -1 on error
    ssize_t rd = fgetxattr(fileFd, key.c_str(), buf, bufLen);
    if (rd < 0)
    {
        delete [] buf;
        return boost::none;
    }

    // check if the buffer was too small
    if (rd > bufLen)
    {
        delete [] buf;
        bufLen = std::min<ssize_t>(rd, 4096);

        buf = new char[bufLen];
        rd = fgetxattr(fileFd, key.c_str(), buf, bufLen);
        if (rd < 0)
        {
            delete [] buf;
            return boost::none;
        }

        rd = std::min<ssize_t>(rd, bufLen);
    }

    std::string result(buf, rd);
    delete [] buf;

    return result;
}

/**
 * @brief Returns the value of the xattr on the file with the given key
 *
 * @sa getXAttrib
 */
boost::optional<std::string> AICommon::getXAttrib(const std::string& filePath, const std::string& key)
{
    int fd = open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        return boost::none;
    }

    boost::optional<std::string> result = AICommon::getXAttrib(fd, key);

    // close the fd and preserve errno across the call
    int errnoSave = errno;
    close(fd);
    errno = errnoSave;

    return result;
}

/**
 * @brief Sets the xattr key value pair on the given file
 *
 * It's recommended that the key string begins with "user.", this
 * is a freedesktop.org recommendation and EXT4 filesystems enforce
 * this, only root can create xattrs with keys that don't begin with
 * "user."
 *
 * This function can fail for many reasons, in which case false is
 * returned and errno contains the error code.  Refer to the setxattr
 * man page for more details.
 *
 * The value is not stored with a null terminator.
 *
 * @param[in] fd     The descriptor to the file or directory to set the
 *                   xattr on.
 * @param[in] key    The key string to write
 * @param[in] value  The value string to write
 * @param[in] flags  Optional flags, see the setxattr man page for
 *                   possible flag values
 *
 * @returns true if the value was written, otherwise false is returned
 * and the error code is in errno
 */
bool AICommon::setXAttrib(int fd, const std::string& key, const std::string& value, int flags /* = 0 */)
{
    return (fsetxattr(fd, key.c_str(), value.data(), value.length(), flags) == 0);
}

bool AICommon::setXAttrib(const std::string& filePath, const std::string& key, const std::string& value, int flags /* = 0 */)
{
    return (setxattr(filePath.c_str(), key.c_str(), value.data(), value.length(), flags) == 0);
}


/**
 * @brief Calculates the MD5 of the file at the given path.
 *
 * On android we can use the MD5 functions in the openssl library.
 *
 *
 * @param[in] filePath  The path to the file to calculate the MD5 on.
 *
 * @returns the MD5 value as a hex string, on failure an empty string is returned.
 */
std::string AICommon::fileMD5(const std::string & filePath)
{
    int fd = open(filePath.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to open file @ '%s'", filePath.c_str());
        return std::string();
    }

    const std::string sum = AICommon::fileMD5(fd);

    if (close(fd) != 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to close file");
    }

    return std::move(sum);
}

#if defined(ANDROID)

/**
 * @brief Calculates the MD5 of the opened file descriptor.
 *
 * @warning This updates the position within the fd, therefore it is not
 * safe to use the fd in multiple threads.
 *
 * On android we can use the MD5 functions in the openssl library.
 *
 * @param[in] fd  The opened file to perform the MD5 over.
 *
 * @returns the MD5 value as a hex string, on failure an empty string is returned.
 */
std::string AICommon::fileMD5(int fd)
{
    const off_t originalPos = lseek(fd, 0, SEEK_CUR);
    if ((originalPos < 0) || (lseek(fd, 0, SEEK_SET) < 0))
    {
        AI_LOG_SYS_ERROR(errno, "Failed to seek to beginning of file");
        return std::string();
    }

    MD5_CTX md5Ctx;
    MD5_Init(&md5Ctx);

    uint8_t buf[1024];
    ssize_t bytesRead;
    do
    {
        bytesRead = TEMP_FAILURE_RETRY(read(fd, buf, sizeof(buf)));
        if (bytesRead < 0)
        {
            AI_LOG_SYS_ERROR(errno, "Error reading file");
            return std::string();
        }
        else if (bytesRead > 0)
        {
            MD5_Update(&md5Ctx, buf, static_cast<size_t>(bytesRead));
        }
    }
    while (bytesRead > 0);

    if (lseek(fd, originalPos, SEEK_SET) < 0)
    {
        AI_LOG_SYS_ERROR(errno, "Failed to restore file position");
    }

    uint8_t result[MD5_DIGEST_LENGTH];
    if (MD5_Final(result, &md5Ctx) != 1)
    {
        AI_LOG_ERROR("Failed to finalise MD5 of file");
        return std::string();
    }

    // now convert to ascii hex
    std::string asciiHex;
    asciiHex.reserve(32 + 1);

    static const char hexChars[17] = "0123456789abcdef";
    for (const uint8_t byte : result)
    {
        asciiHex += hexChars[(byte >> 4) & 0xf];
        asciiHex += hexChars[(byte >> 0) & 0xf];
    }

    return asciiHex;
}

#else // !defined(ANDROID)

std::string AICommon::fileMD5(int fd)
{
    AI_LOG_FN_ENTRY();

    char tempBuffer[1024];
    AI_MD5_CTX md5Ctx;

    AI_MD5_Init(&md5Ctx);

    ssize_t bytesRead = 0, totalBytes = 0;

    do
    {
        bytesRead = TEMP_FAILURE_RETRY(read(fd, tempBuffer, sizeof(tempBuffer)));
        if (bytesRead > 0)
        {
            totalBytes += bytesRead;
            AI_MD5_Update(&md5Ctx, tempBuffer, bytesRead);
        }
    }
    while (bytesRead > 0);

    unsigned char md5Text[AI_MD5_DIGEST_LENGTH];

    AI_MD5_Final(md5Text, &md5Ctx);

    // TODO:Move to string
    std::string asciiHex;

    // now convert to ascii hex
    static const char hexChars[17] = "0123456789abcdef";
    for (const unsigned char byte : md5Text)
    {
        asciiHex += hexChars[(byte >> 4) & 0xf];
        asciiHex += hexChars[(byte >> 0) & 0xf];
    }

//        AI_LOG_ERROR("md5 for file %s is %s", filePath.c_str(), asciiHex.c_str());
    AI_LOG_FN_EXIT();
    return asciiHex;
}

#endif  // !defined(ANDROID)

int AICommon::getDeviceFreeMegabytes(const std::string & path)
{
    struct statvfs stats;

    if (statvfs(path.c_str(), &stats) == 0)
    {
        return int((uint64_t(stats.f_bsize) * uint64_t(stats.f_bavail)) / 1048576);
    }
    return -1;
}
