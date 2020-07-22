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
 * File:   DobbyRootfs.h
 *
 */
#ifndef DOBBYROOTFS_H
#define DOBBYROOTFS_H

#include "IDobbyUtils.h"
#include "ContainerId.h"

#include "DobbySpecConfig.h"
#include "DobbyBundleConfig.h"

#include <sys/types.h>

#include <memory>
#include <string>


class DobbyBundle;

// -----------------------------------------------------------------------------
/**
 *  @class DobbyRootfs
 *  @brief Creates a directory populated with rootfs based on the supplied
 *  container config.
 *
 *  At construction time a directory is created within the bundle named 'rootfs'
 *
 *  It is then populated with any static files as indicated by the container
 *  config object.
 *
 *  At destruction time the rootfs and all it's contents are deleted.
 */
class DobbyRootfs
{
public:
    DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,
                const std::shared_ptr<const DobbyBundle>& bundle,
                const std::shared_ptr<const DobbySpecConfig>& config);
    DobbyRootfs(const std::shared_ptr<IDobbyUtils>& utils,
                const std::shared_ptr<const DobbyBundle>& bundle,
                const std::shared_ptr<const DobbyBundleConfig>& config);
    ~DobbyRootfs();

public:
    bool isValid() const;

    const std::string& path() const;
    int dirFd() const;

    void setPersistence(bool persist);

private:
    void cleanUp(bool dontRemoveFiles);

    bool createMountPoint(int dirfd, const std::string &path, bool isDirectory) const;
    bool createStandardMountPoints(int dirfd) const;

    bool constructRootfs(int dirfd,
                         const std::shared_ptr<const DobbySpecConfig>& config);

    bool createAndWriteFileAt(int dirFd,
                              const std::string& filePath,
                              const std::string& fileContents,
                              mode_t mode = 0644) const;

    void unmountAllAt(const std::string& pathPrefix);

private:
    const std::shared_ptr<IDobbyUtils> mUtilities;
    const std::shared_ptr<const DobbyBundle> mBundle;
    std::string mPath;
    int mDirFd;
    bool mPersist;
};


#endif // !defined(DOBBYROOTFS_H)
