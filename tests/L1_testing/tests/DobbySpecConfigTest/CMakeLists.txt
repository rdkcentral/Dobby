# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Sky UK
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
project(DobbySpecConfigL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)
find_package(ctemplate REQUIRED)
find_package(jsoncpp REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

# Real sources only – no PIMPL mock wrappers for DobbyBundle or DobbyTemplate.
# DobbySettingsMock is the only mock used; it implements IDobbySettings via gmock.
add_library(DobbySpecConfigTestLib SHARED STATIC
            ../../../../bundle/lib/source/DobbySpecConfig.cpp
            ../../../../bundle/lib/source/DobbyTemplate.cpp
            ../../../../bundle/lib/source/DobbyBundle.cpp
            ../../../../AppInfrastructure/Logging/source/Logging.cpp
            ../../mocks/DobbyConfigMock.cpp
            ../../mocks/IpcFileDescriptorMock.cpp
            ../../mocks/rt_dobby_schema.c
            DobbySpecConfigLinkStubs.cpp
            )

# Real bundle/lib headers MUST come before the mocks directory so that the
# real DobbyTemplate.h (with ctemplate members) is found instead of the
# stripped-down mock version.
target_include_directories(DobbySpecConfigTestLib
                PUBLIC
                ../../../../bundle/lib/include
                ../../../../bundle/lib/source
                ../../../../utils/include
                ../../../../utils/source
                ../../../../AppInfrastructure/Logging/include
                ../../../../AppInfrastructure/Common/include
                ../../../../ipcUtils/include
                ../../../../settings/include
                ../../../../daemon/lib/include
                ../../../../libocispec/generated_output
                ../../../../pluginLauncher/lib/include
                ../../../../protocol/include
                ../../../../build/AppInfrastructure/Tracing
                ../../mocks
                /usr/include/jsoncpp
                )

file(GLOB TESTS *.cpp)

add_executable(${PROJECT_NAME} ${TESTS})

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    DobbySpecConfigTestLib
    GTest::gmock
    GTest::GTest
    GTest::Main
    ctemplate
    pthread
    jsoncpp
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
