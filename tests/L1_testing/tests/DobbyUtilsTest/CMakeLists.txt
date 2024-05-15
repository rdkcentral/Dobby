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
project(DobbyUtilsL1Test)

set(CMAKE_CXX_STANDARD 14)

find_package(GTest REQUIRED)
find_package(ctemplate REQUIRED)
find_package(jsoncpp REQUIRED)

include_directories(${GTEST_INCLUDE_DIRS})


add_library(Utils SHARED
            STATIC
            ../../../../utils/source/DobbyUtils.cpp
            ../../../../utils/source/ContainerId.cpp
            ../../../../utils/source/DobbyTimer.cpp
            ../../../../AppInfrastructure/Logging/source/Logging.cpp
            )


target_include_directories(Utils
                PUBLIC
                ../../../../utils/include
                ../../../../utils/source
                ../../../../AppInfrastructure/Logging/include
                ../../../../AppInfrastructure/Common/include
                )

file(GLOB TESTS *.cpp)

add_executable(${PROJECT_NAME} ${TESTS})
target_link_libraries(${PROJECT_NAME} Utils ${GTEST_LIBRARIES} gtest_main pthread)

#target_link_libraries(${PROJECT_NAME} ${GTEST_LIBRARIES} GTest::GTest GTest::Main  gmock gmock_main Utils ctemplate pthread jsoncpp systemd dl dbus-1)
#target_link_libraries(${PROJECT_NAME} Utils  ${GTEST_LIBRARIES} gtest_main  gmock_main ctemplate pthread jsoncpp systemd dl dbus-1)


install(TARGETS ${PROJECT_NAME} DESTINATION bin)
