# If not stated otherwise in this file or this component's license file the
# following copyright and licenses apply:
#
# Copyright 2020 RDK Management
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

#
# Find libyajl
#
# YAJL_FOUND          - true if yajl was found
# YAJL_INCLUDE_DIR    - where to find the yajl headers
# YAJL_LIBRARY        - where to find the library libyail
#

include(FindPackageHandleStandardArgs)

# Check for the header files

# For some reason in Bitbake, only yajl2 has the _version.h file, so if we find that,
# we know we have v2.
find_path(YAJL_INCLUDE_DIR NAMES yajl_version.h PREFIX yajl yajl2)

# Search for libyajl2.so first - that's the version we want when building with
# bitbake.
find_library(YAJL_LIBRARY NAMES yajl2)
find_library(YAJL_LIBRARY NAMES yajl)

find_package_handle_standard_args(YAJL DEFAULT_MSG
    YAJL_LIBRARY YAJL_INCLUDE_DIR
)

mark_as_advanced( YAJL_INCLUDE_DIR YAJL_LIBRARY )

if(YAJL_FOUND)
    set(YAJL_INCLUDE_DIRS ${YAJL_INCLUDE_DIR})
    set(YAJL_LIBRARIES ${YAJL_LIBRARY})

    add_library(yajl UNKNOWN IMPORTED)
    set_target_properties(yajl PROPERTIES
        IMPORTED_LOCATION "${YAJL_LIBRARY}"
        IMPORTED_INCLUDE_DIRECTORIES "${YAJL_INCLUDE_DIR}"
    )
endif()