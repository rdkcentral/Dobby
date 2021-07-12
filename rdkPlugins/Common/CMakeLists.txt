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
project(DobbyRdkPluginCommonLib)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
    INTERFACE

    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>

    PRIVATE
    $<TARGET_PROPERTY:DobbyDaemonLib,INTERFACE_INCLUDE_DIRECTORIES>
)

target_link_libraries(${PROJECT_NAME}
    INTERFACE
    DobbyPluginLauncherLib
)

install(TARGETS DobbyRdkPluginCommonLib
        EXPORT DobbyTargets
        ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
        PUBLIC_HEADER DESTINATION include/Dobby
)

# Install public headers for external use
install( DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Dobby/rdkPlugins/
        FILES_MATCHING PATTERN "*.h"
)