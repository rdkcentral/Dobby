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

add_library( DobbyClientLib

        STATIC
        source/DobbyFactory.cpp
        source/DobbyProxy.cpp
        source/Upstart.cpp

        )

set_target_properties(DobbyClientLib PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_include_directories( DobbyClientLib

        PUBLIC

        # Adds the include dirs to INTERFACE_INCLUDE_DIRECTORIES for exporting
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../protocol/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../AppInfrastructure/Public>

        )

target_link_libraries(DobbyClientLib
        IpcService
        AppInfraLogging
        AppInfraCommon
)


# Add include files to install to use DobbyProxy externally
list( APPEND INCLUDE_FILES
        "${PROJECT_SOURCE_DIR}/protocol/include/DobbyProtocol.h"
        "${PROJECT_SOURCE_DIR}/client/lib/include/DobbyProxy.h"
        "${PROJECT_SOURCE_DIR}/AppInfrastructure/Common/include/IDGenerator.h"
)
set_target_properties( DobbyClientLib PROPERTIES PUBLIC_HEADER "${INCLUDE_FILES}" )

install(TARGETS DobbyClientLib
        EXPORT DobbyTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        PUBLIC_HEADER DESTINATION include/Dobby
)

# Install public headers from /AppInfrastructure/Public
install( DIRECTORY "${PROJECT_SOURCE_DIR}/AppInfrastructure/Public/"
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Dobby/Public
        FILES_MATCHING PATTERN "*.h"
)
