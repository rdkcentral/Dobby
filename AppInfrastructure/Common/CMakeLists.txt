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

add_library( AppInfraCommon

        STATIC
        source/FileUtilities.cpp
        source/Timer.cpp
        source/PollLoop.cpp
        source/AI_MD5.c
        )

set_target_properties(AppInfraCommon PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories( AppInfraCommon

        PUBLIC
        # Adds the include dirs to INTERFACE_INCLUDE_DIRECTORIES for exporting
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../Public/Common>

        PRIVATE
        ${Boost_INCLUDE_DIRS}
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>

        )

install(TARGETS AppInfraCommon
        EXPORT DobbyTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"

        )
