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

# - Try to find the Perfetto SDK.
#
# SDK download can be found here: https://perfetto.dev/docs/instrumentation/tracing-sdk
# with a sample CMake file to build the SDK. This file has sane defaults but
# modify this file as necessary to find the Perfetto SDK in your build environment

find_path( PERFETTO_INCLUDE_DIR NAMES perfetto.h PREFIX perfetto)
find_library( PERFETTO_LIBRARY NAMES libPerfettoSdk.a PERFETTO )

#message( "PERFETTO_INCLUDE_DIR include dir = ${PERFETTO_INCLUDE_DIR}" )
#message( "PERFETTO_LIBRARY lib = ${PERFETTO_LIBRARY}" )

include( FindPackageHandleStandardArgs )

# Handle the QUIETLY and REQUIRED arguments and set the PERFETTO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args( PERFETTO DEFAULT_MSG
        PERFETTO_LIBRARY PERFETTO_INCLUDE_DIR )

mark_as_advanced( PERFETTO_INCLUDE_DIR PERFETTO_LIBRARY )

if( PERFETTO_FOUND )
    set( PERFETTO_LIBRARIES ${PERFETTO_LIBRARY} )
    set( PERFETTO_INCLUDE_DIRS ${PERFETTO_INCLUDE_DIR} )
endif()

if( PERFETTO_FOUND AND NOT TARGET PERFETTO::PERFETTO )
    add_library( PERFETTO::PERFETTO SHARED IMPORTED )
    set_target_properties( PERFETTO::PERFETTO PROPERTIES
            IMPORTED_LOCATION "${PERFETTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PERFETTO_INCLUDE_DIRS}" )
endif()