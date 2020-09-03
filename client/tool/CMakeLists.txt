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


add_executable( # Sets the name of the exec.
        DobbyTool

        # The entry point
        source/Main.cpp

        # Import AI components
        $<TARGET_OBJECTS:AppInfraReadLine>

        )

# Specifies libraries CMake should link to your target binary. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_include_directories(
        DobbyTool

        PRIVATE
        ../lib/source
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraCommon,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraReadLine,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

)

# Specifies libraries CMake should link to your target binary. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

target_link_libraries( # Specifies the target exec
        DobbyTool

        DobbyClientLib
        IpcService
        AppInfraLogging
        AppInfraCommon
        AppInfraTracing

        # Import the 3rd party libraries
        JsonCpp::JsonCpp
        systemd

        # Adds pthread support (if it's a separate library on target)
        Threads::Threads

        # Adds dlopen support (if it's a separate library on target)
        ${CMAKE_DL_LIBS}

        )

install( # Specifies the target to install
        TARGETS DobbyTool

        RUNTIME DESTINATION bin

        )
