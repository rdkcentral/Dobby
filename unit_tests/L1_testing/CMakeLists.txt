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

cmake_minimum_required(VERSION 3.7)
project(DobbyL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)
find_package(ctemplate REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_library(Utils SHARED
            STATIC
            ../../utils/source/DobbyUtils.cpp
            ../../utils/source/ContainerId.cpp
            ../../utils/source/DobbyTimer.cpp
            ../../AppInfrastructure/Logging/source/Logging.cpp
            ../../daemon/lib/source/Dobby.cpp
            ../../daemon/lib/source/DobbyManager.cpp
            )

target_include_directories(Utils
                PUBLIC
                ../../utils/include
                ../../utils/source
                ../../AppInfrastructure/Logging/include
                ../../AppInfrastructure/Common/include
                ../../bundle/lib/include
                ../../ipcUtils/include
                ../../settings/include
                ../../daemon/lib/source
                ../../AppInfrastructure/IpcService/include
                ../../libocispec/generated_output
                ../../pluginLauncher/lib/include
                ../../daemon/lib/include
                ../../protocol/include
                ../../build/AppInfrastructure/Tracing
                ../../AppInfrastructure/IpcService/source/libdbus
                ../../rdkPlugins/Networking/include
                /usr/lib/plugins/dobby
                /usr/include/jsoncpp
                /usr/include/dbus-1.0
                /usr/lib/x86_64-linux-gnu/dbus-1.0/include
                mocks
                )

file(GLOB TESTS tests/*.cpp)

add_executable(${PROJECT_NAME} ${TESTS})

target_link_libraries(${PROJECT_NAME} Utils ${GTEST_LIBRARIES} gtest_main ctemplate pthread)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
