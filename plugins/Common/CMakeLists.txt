# If not stated otherwise in this file or this component's license file the
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

add_library( DobbyPluginCommonLib

        OBJECT
        source/ServiceMonitor.cpp

        )

# The following enables the -fPIC flag on the object files. We need this as the
# code here is expected to be linked into plugin / shared libraries
set_target_properties( DobbyPluginCommonLib

        PROPERTIES
        POSITION_INDEPENDENT_CODE ON

        )

if( BUILDING_WITH_RDK_SDK )

    add_dependencies( DobbyPluginCommonLib JsonCpp::JsonCpp )

endif()

target_include_directories( DobbyPluginCommonLib

        PUBLIC
        include
        $<TARGET_PROPERTY:JsonCpp::JsonCpp,INTERFACE_INCLUDE_DIRECTORIES>

        PRIVATE
        $<TARGET_PROPERTY:DobbyDaemonLib,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>
        $<TARGET_PROPERTY:AppInfraLogging,INTERFACE_INCLUDE_DIRECTORIES>

        )

