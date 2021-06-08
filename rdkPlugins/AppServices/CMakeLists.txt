# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2021 Sky UK
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

project(AppServicesRdkPlugin)

add_library( ${PROJECT_NAME}
        SHARED
        source/AppServicesRdkPlugin.cpp
        ../Networking/source/Netfilter.cpp
        ../Networking/source/StdStreamPipe.cpp
        )

target_include_directories( AppServicesRdkPlugin
        PRIVATE
        ../Networking/include
        )

install(
        TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib/plugins/dobby
        NAMELINK_SKIP
        )

target_link_libraries(${PROJECT_NAME}
        DobbyRdkPluginCommonLib
)

set_target_properties( ${PROJECT_NAME} PROPERTIES SOVERSION 1 )
