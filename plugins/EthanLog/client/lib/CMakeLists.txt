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

add_library( ethanlog
        SHARED
        source/ethanlog.c

        )

target_include_directories( ethanlog
        PUBLIC

        # Adds the include dir to INTERFACE_INCLUDE_DIRECTORIES for exporting
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        )


set_target_properties( ethanlog
        PROPERTIES
        PUBLIC_HEADER include/ethanlog.h
        VERSION       ${DOBBY_MAJOR_VERSION}.1.0
        SOVERSION     ${DOBBY_MAJOR_VERSION}

        )


# Install the library and headers in the usual places
include( GNUInstallDirs )
install( TARGETS ethanlog
         EXPORT ethanlog-export
         LIBRARY DESTINATION lib
         PUBLIC_HEADER DESTINATION include
        )

# Install the export targets for others to import
install( EXPORT ethanlog-export
         FILE ethanlog-targets.cmake
         NAMESPACE ethanlog::
         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ethanlog
        )

# Create a ConfigVersion.cmake file
include(CMakePackageConfigHelpers)
write_basic_package_version_file(
         ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config-version.cmake
         VERSION ${PROJECT_VERSION}
         COMPATIBILITY AnyNewerVersion
        )

configure_package_config_file(
        ${CMAKE_CURRENT_LIST_DIR}/ethanlog-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config.cmake
         INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ethanlog
        )

# Install the config, configversion and custom find modules
install( FILES       ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config.cmake
                     ${CMAKE_CURRENT_BINARY_DIR}/ethanlog-config-version.cmake
         DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ethanlog
        )

