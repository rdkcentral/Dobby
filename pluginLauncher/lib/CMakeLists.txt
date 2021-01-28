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

project(DobbyPluginLauncherLib)

add_library(${PROJECT_NAME} STATIC
    source/DobbyRdkPluginManager.cpp
    source/DobbyRdkPluginUtils.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    include

    PRIVATE
    $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:AppInfraCommon,INTERFACE_INCLUDE_DIRECTORIES>
)

set_target_properties( ${PROJECT_NAME}
    PROPERTIES POSITION_INDEPENDENT_CODE ON
)

target_link_libraries(${PROJECT_NAME}
    AppInfraLogging
    AppInfraCommon

    DobbyDaemonLib
    Threads::Threads

    libocispec

    # 3rd party libraries
    JsonCpp::JsonCpp

    # Adds dlopen support (if it's a separate library on target)
    ${CMAKE_DL_LIBS}
)

set_target_properties( ${PROJECT_NAME}
        PROPERTIES POSITION_INDEPENDENT_CODE ON
)
