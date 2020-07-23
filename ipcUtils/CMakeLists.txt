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

project(DobbyIpcUtils)

add_library(${PROJECT_NAME}
    STATIC
    source/DobbyIPCUtils.cpp
    source/DobbyIpcBus.cpp
)

target_include_directories(${PROJECT_NAME}
    PUBLIC
    include
    $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

    PRIVATE
    ../protocol/include
    $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:AppInfraCommon,INTERFACE_INCLUDE_DIRECTORIES>
    $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

    ${DBUS_INCLUDE_DIRS}
)