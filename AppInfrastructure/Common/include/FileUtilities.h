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
 * File:   FileUtilities.h
 * Author: jarek.dziedzic@bskyb.com
 *
 * Created on 11 July 2014
 *
 */

#ifndef FILEUTILITIES_H
#define FILEUTILITIES_H

#include <string>
#include <vector>
#include <dirent.h>
#include <sys/stat.h>
#include <stdio.h>
#include <limits.h>

#include <boost/optional.hpp>


#if !defined(SIZE_T_MAX)
#  define SIZE_T_MAX SIZE_MAX
#endif


namespace AICommon
{

const mode_t S_IRXALL  = S_IRUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH;
const mode_t S_IRALL  = S_IRUSR | S_IRGRP | S_IROTH;

/**
 * @brief splits /a/path/to/somewhere to {a, path, to, somewhere}
 * @note This function works for both absolute and relative paths.
 */
std::vector<std::string> splitPath(const std::string& path);

/**
 * @brief Recursively creates directories. Equivalent of mkdir -p in bash.
 * @param path - a path to the directory to be created. Can be relative or absolute
 * @param mode - the file access mode to create the directory with (only applied to created directories)
 * @return true if the directories were created successfully.
 * @note Existing directories are not a cause for error.
 */
bool mkdirRecursive(const std::string& path, mode_t mode = S_IRWXU);

/**
 * @brief Recursively creates directories within the directory fd.
 * @param path - a path to the directory to be created. Absolute paths are treats as abs to parentFd
 * @param mode - the file access mode to create the directory with (only applied to created directories)
 * @return true if the directories were created successfully.
 * @note Existing directories are not a cause for error.
 */
bool mkdirRecursiveAt(const std::string& fullPath, int parentDirectoryFd, mode_t mode = S_IRWXU);

/**
 * @brief Returns true if path points to a file, directory or symlink.
 */
bool exists(const std::string& path);

/**
 * @brief copyFile
 * @param to source path of file to copy
 * @param from destination path
 * @return
 */
int copyFile(const std::string & to, const std::string & from);
int copyFile(const char *to, const char *from);


/**
 * @brief Deletes the directory and all the sub files/directories.
 *
 * @param   directoryName   The name of the directory.
 */
void deleteDirectory(const std::string& directoryName);

/**
 * @brief Deletes the directory and all the sub files/directories.
 *
 * @param   parentDirectoryFd An fd corresponding to the parent dir.
 * @param   directoryName     The name of the directory within parentDirectoryFd.
 */
void deleteDirectoryAt(int parentDirectoryFd, const std::string& directoryName);

/**
 * @brief Deletes the files inside the directory
 *
 * @param   directoryName   The name of the directory.
 *
 * @return Returns true if successfully deleted files
 */
 bool deleteFilesInDirectory(const std::string& directoryName);

/**
 * @brief deleteFile
 * @param filePath
 * @return Remove the file at the given location
 */
bool deleteFile(const std::string & filePath);

/**
* @brief getFilesInDirectory
 * @param dirName - the fully qualified path to the directory
 * @return Returns names of all files (and only files) in the specified directory
 */
std::vector<std::string> getFilesInDirectory(int dirFd);
std::vector<std::string> getFilesInDirectory(const std::string & dirName, bool fullPaths = false);

/**
 * @brief A C++ wrapper for standard realpath. Resolves paths to canonical form.
 * @return resolved path
 * @throws std::invalid_argument if the input path is invalid (e.g. doesn't exist)
 */
std::string resolvePath(const std::string& in);

/**
 * @brief Reads entire contents of the file to memory.
 * @note if the file doesn't exist, this function returns empty vector.
 */
std::vector<uint8_t> fileContents(const std::string& filepath);

/**
 * @brief Reads entire contents of the file to memory.
 * @note if an error occurs reading the file or the size is too big an empty
 * vector is returned.
 */
std::vector<uint8_t> fileContents(int fd, size_t maxSize = SIZE_T_MAX);
std::vector<uint8_t> fileContents(int dirFd, const std::string& filepath, size_t maxSize = SIZE_T_MAX);

/**
 * @brief A simple file comparator function
 * @return true if exactly the same. false otherwise or on ANY failure
 */
bool compareFilesExactly(const std::string & filePathApples, const std::string & filePathOranges);

/**
 * @brief Creates a simple text file at the given path with the given contents, will truncate the file if it exists
 * @param filePath
 * @param contents
 * @param mode
 * @return true if the file was written
 */
bool createTextFile(const std::string& filePath, const std::string& contents, mode_t mode = S_IRWXU);
bool createTextFileAt(int dirFd, const std::string& filePath, const std::string& contents, mode_t mode = S_IRWXU);

/**
 * Read all the bytes from the FILE stream fp
 */
std::string readTextStream(FILE *fp);

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
boost::optional<std::string> getXAttrib(int fileFd, const std::string& key);

/**
 * @brief Returns the value of the xattr on the file with the given key
 *
 * @sa getXAttrib
 */
boost::optional<std::string> getXAttrib(const std::string& filePath, const std::string& key);

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
bool setXAttrib(int fd, const std::string& key, const std::string& value, int flags = 0);
bool setXAttrib(const std::string& filePath, const std::string& key, const std::string& value, int flags = 0);

/**
 * @brief fileMD5 Returns the MD5 of the package as an ASCII HEX string
 * @param filePath The file from which to take the hash
 * @return The hash code (MD5 as ASCII HEX)
 */
std::string fileMD5(const std::string & filePath);
std::string fileMD5(int fd);

/**
 * @brief getDeviceFreeMegabytes Returns how many megabytes of free space are available on a device
 * @param path The path of a file/folder used to identify which device to get information
 * @return The number of megabytes free on the device in which "path" is resident.
 * The value is negative if there is not enough space free
 * Note - there is an upper limit on the amount of free space that can be returned
 * This is ((2^31)-1) megabytes or 2251799812636672 bytes approx 2 petabytes.
 */
int getDeviceFreeMegabytes(const std::string & path);

/**
 * @brief getDirectorySizeInKb Returns how many kilobytes of space is used up in the directory
 * @param path The path of a folder used to get the size of
 * @return The number of kilobytes used up on the directory in which "path" is resident.
 */
size_t getDirectorySizeInKb( const std::string &path );

}

#endif  /* FILEUTILITIES_H */

