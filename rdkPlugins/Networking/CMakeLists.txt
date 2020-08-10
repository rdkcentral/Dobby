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

project(NetworkingPlugin)


add_library( ${PROJECT_NAME}
        MODULE
        source/NetworkingPlugin.cpp
        source/NetworkSetup.cpp
        source/BridgeInterface.cpp
        source/Netlink.cpp
        source/Netfilter.cpp
        source/DnsmasqSetup.cpp
        source/NetworkingHelper.cpp
        source/HolePuncher.cpp
        source/LoPorts.cpp

        # proxy to talk to DobbyDaemon
        ../Common/source/DobbyRdkPluginProxy.cpp
)

install(
        TARGETS ${PROJECT_NAME}
        LIBRARY DESTINATION lib/plugins/dobby
)

target_link_libraries( ${PROJECT_NAME}
        DobbyRdkPluginCommonLib
        IpcService

        # 3rd party libraries
        systemd
        Threads::Threads
        LIBNL::libnl
        LIBNL::libnl-route
)

target_include_directories( ${PROJECT_NAME}
        PUBLIC
        include

        PRIVATE
        ${LIBNL_INCLUDE_DIR}
        ../../protocol/include
        $<TARGET_PROPERTY:IpcService,INTERFACE_INCLUDE_DIRECTORIES>

)

# uncomment to use workaround functions for libnl bridge creation
#target_compile_definitions( ${PROJECT_NAME} PRIVATE ENABLE_LIBNL_BRIDGE_WORKAROUND )
