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
 * File:   DobbyFileAccessFixer.h
 *
 */
#ifndef DOBBYFILEACCESSFIXER_H
#define DOBBYFILEACCESSFIXER_H

#include <ftw.h>
#include <sys/stat.h>
#include <sys/types.h>


// -----------------------------------------------------------------------------
/**
 *  @class DobbyFileAccessFixer
 *  @brief Utility object to fix the various incorrectly 'hardened' file
 *  permissions.
 *
 *  The 'hardening' process continuously 'over hardens' various files to the
 *  point where things become unusable.  This object is used to go through and
 *  fix-up the files before launching the DobbyDaemon.
 *
 *  This method only has one method, fixIt() that applies all the know file
 *  perms fixups.
 *
 *  Hopefully in the future we can remove all these hacks and have just the
 *  correct perms from the start.
 *
 */
class DobbyFileAccessFixer
{
public:
    DobbyFileAccessFixer();
    ~DobbyFileAccessFixer();

public:
    bool fixIt() const;

private:
    bool fixDobbyInitPerms() const;
    bool fixOptRuntimePerms() const;
    bool fixGfxDriverPerms() const;
    bool fixCoreDumpFilter() const;

private:
    static void chmodFile(const char* filePath, mode_t oldPerms, mode_t newPerms);
    static int fixRuntimePerms(const char* filePath, const struct stat* statBuf,
                               int typeFlag, struct FTW* ftwbuf);

};


#endif // !defined(DOBBYFILEACCESSFIXER_H)
