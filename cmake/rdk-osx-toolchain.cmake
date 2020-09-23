# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2020 Sky UK
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# boilerplate
set( CMAKE_SYSTEM_NAME Linux )
set( CMAKE_SYSTEM_VERSION 1 )

set( RDK_TOOLCHAIN_BASE "/Volumes/Katalix/RDK/firebolt/osx-sdk-xi6-latest" )
set( RDK_TOOLCHAIN_PREFIX ${RDK_TOOLCHAIN_BASE}/bin/arm-rdk-linux-gnueabi- )

# compiler flags as set by the RDK SDK
set( RDK_DEFAULT_CFLAGS "-march=armv7ve -mfpu=neon  -mfloat-abi=hard -mcpu=cortex-a15 -fno-omit-frame-pointer -fno-optimize-sibling-calls -Os " )

# specify the cross compiler
set( CMAKE_C_COMPILER   ${RDK_TOOLCHAIN_PREFIX}gcc )
set( CMAKE_CXX_COMPILER ${RDK_TOOLCHAIN_PREFIX}g++ )
set( CMAKE_AR           ${RDK_TOOLCHAIN_PREFIX}ar  )

# the following is needed as the OSX toolchain uses the "/lib/ld-linux.so.3"
# linker path, whereas in the RDK rootfs they haven't added the symlink so
# instead we force it to be the original file
# (nb if you run 'file' on the built binary you can see the difference)
set( CMAKE_EXE_LINKER_FLAGS "-Wl,--dynamic-linker,/lib/ld-2.24.so" )

set( CMAKE_C_FLAGS "${RDK_DEFAULT_CFLAGS}" CACHE STRING "" FORCE )
set( CMAKE_CXX_FLAGS "${RDK_DEFAULT_CFLAGS}" CACHE STRING "" FORCE )

# where is the target environment
set( CMAKE_SYSROOT "${RDK_TOOLCHAIN_BASE}/arm-rdk-linux-gnueabi/sysroot" )
set( CMAKE_FIND_ROOT_PATH "${RDK_TOOLCHAIN_BASE}/arm-rdk-linux-gnueabi/sysroot" )

# there seems to be an issue with --sysroot from CMAKE_SYSROOT when checking
# if the compiler works, so for now assume the compiler works
set( CMAKE_C_COMPILER_WORKS TRUE )
set( CMAKE_CXX_COMPILER_WORKS TRUE )

# search for programs in the build host directories
set( CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER )

# for libraries and headers in the target directories
set( CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY )
set( CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY )

# temporary flag used to determine where we pull the open source libraries from
set( BUILDING_WITH_RDK_SDK TRUE )
