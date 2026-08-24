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
project(DobbyProxyL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})

add_library(DobbyProxyTestLib STATIC
    ../../../../client/lib/source/DobbyProxy.cpp
    ../../../../AppInfrastructure/Logging/source/Logging.cpp
    ../../mocks/IIpcServiceMock.cpp
    ../../mocks/IpcVariantList.cpp
    ../../mocks/IpcFileDescriptorMock.cpp
)

target_include_directories(DobbyProxyTestLib
    PUBLIC
    ../../mocks
    ../../../../client/lib/include
    ../../../../AppInfrastructure/Public
    ../../../../AppInfrastructure/Common/include
    ../../../../AppInfrastructure/Logging/include
    ../../../../AppInfrastructure/IpcService/include
    ../../../../protocol/include
)

file(GLOB TESTS *.cpp
    ../../mocks/IpcVariantList.cpp
)

add_executable(${PROJECT_NAME} ${TESTS})

target_include_directories(${PROJECT_NAME}
    PRIVATE
    ../../mocks
    ../../../../client/lib/include
    ../../../../AppInfrastructure/Public
    ../../../../AppInfrastructure/Common/include
    ../../../../AppInfrastructure/Logging/include
    ../../../../AppInfrastructure/IpcService/include
    ../../../../protocol/include
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    DobbyProxyTestLib
    GTest::gmock
    GTest::GTest
    GTest::Main
    pthread
)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
