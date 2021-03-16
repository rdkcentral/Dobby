# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:

# Copyright 2020 Sky UK

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

# http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Build and run the libocispec generator tool to create header files
# Will also build parsers for any schemas in the bundle/schemas directory

# TODO:: Do this in a more "bitbake-like" way instead of with submodules

# ------------------------------------------------------------------------------

if(DOBBY_RUNTIME_SPEC)
    set(SCHEMAS_DIR ${DOBBY_RUNTIME_SPEC})
else()
    set(SCHEMAS_DIR ${CMAKE_SOURCE_DIR}/bundle/runtime-schemas)
endif()

if(DOBBY_LIBOCISPEC_ROOT)
    set(LIBOCISPEC_DIR ${DOBBY_LIBOCISPEC_ROOT})
else()
    set(LIBOCISPEC_DIR ${CMAKE_SOURCE_DIR}/libocispec)
endif()

function(GenerateLibocispec)
    # Configure function arguments
    set(multiValueArgs EXTRA_SCHEMA_PATH)

    cmake_parse_arguments(Argument "" "" "${multiValueArgs}" ${ARGN} )

    if(Argument_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to GenerateLibocispec(): \"${Argument_UNPARSED_ARGUMENTS}\"")
    endif()

    message("Updating libocispec submodule")
    # Make sure the git submodule contains the nested submodules properly
    execute_process(
        COMMAND git submodule update --init --recursive
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}
    )

    message("Cleaning libocispec")
    # Clean the libocispec directory to remove old generated code
    execute_process(
        COMMAND git clean -dfx -e "*.py"
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}
    )
    execute_process(
        COMMAND git clean -dfx
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}/runtime-spec
    )

    # Get a list of all the JSON customs schema files and copy them into libocispec
    file(GLOB CUSTOM_SCHEMA_FILES CONFIGURE_DEPENDS "${SCHEMAS_DIR}/*.json")
    file(COPY ${CUSTOM_SCHEMA_FILES} DESTINATION ${LIBOCISPEC_DIR}/runtime-spec/schema/)

    if(Argument_EXTRA_SCHEMA_PATH)
        # We've got extra schemas from outside the Dobby repo
        foreach(_schema_file ${Argument_EXTRA_SCHEMA_PATH})
            message("Adding external schema file from ${_schema_file}")
            list(APPEND _extraschemas "${_schema_file}")
        endforeach(_schema_file)

        # Copy to libocispec directory
        file(COPY ${_extraschemas} DESTINATION ${LIBOCISPEC_DIR}/runtime-spec/schema/)

        execute_process(
            WORKING_DIRECTORY ${SCHEMAS_DIR}
            COMMAND python3 add_external_plugin_schema.py --dobbyschema=${LIBOCISPEC_DIR}/runtime-spec/schema/dobby_schema.json ${_extraschemas}
        )
    else()
        # This will reset the schema back to default
        execute_process(
            WORKING_DIRECTORY ${SCHEMAS_DIR}
            COMMAND python3 add_external_plugin_schema.py --dobbyschema=${LIBOCISPEC_DIR}/runtime-spec/schema/dobby_schema.json
        )
    endif()

    # Remove any schemas that Dobby doesn't use and therefore are pointless to generate
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/config-solaris.json)
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/config-windows.json)
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/config-vm.json)
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/defs-solaris.json)
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/defs-windows.json)
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/defs-vm.json)

    # We use Dobby schema instead, which is almost identical but has the RDKPlugins additional section
    file(REMOVE ${LIBOCISPEC_DIR}/runtime-spec/schema/config-schema.json)


    # Where the libocispec generated C files will live
    set(LIBOCISPEC_GENERATED_DIR
        ${LIBOCISPEC_DIR}/generated_output
    )

    # Get the add_plugin_tables.py script and copy it into libocispec
    file(COPY "${SCHEMAS_DIR}/add_plugin_tables.py" DESTINATION ${LIBOCISPEC_GENERATED_DIR}/)

    message("Configuring libocispec")
    # Actually build libocispec with our custom schemas added
    # Run autotools to make sure everything is ready (and create config.h)
    execute_process(
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}
        COMMAND ${LIBOCISPEC_DIR}/autogen.sh "--host=x86_64-pc-linux-gnu"
    )

    # Change the names of the output source files to reduce the length of variable and function names
    execute_process(
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}
        COMMAND mkdir -p ./schemas/rt
    )

    execute_process(
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}
        COMMAND cp -r ./runtime-spec/schema/. ./schemas/rt/
    )

    # Now run the generator to make our code
    execute_process(
        WORKING_DIRECTORY ${LIBOCISPEC_DIR}
        COMMAND python3 ./src/generate.py --gen-common --gen-ref --root=./schemas --out=${LIBOCISPEC_GENERATED_DIR} ./schemas/rt
    )

    # DobbyConfig needs to be able to see a list of plugins' names and pointers to their structs
    # Add plugin tables to Dobby's schema
    execute_process(
        WORKING_DIRECTORY ${LIBOCISPEC_GENERATED_DIR}
        COMMAND python3 add_plugin_tables.py
    )

    # Get the generated C files
    file(GLOB_RECURSE LIBOCISPEC_GENERATED_FILES ${LIBOCISPEC_GENERATED_DIR}/*.c)
    list(APPEND LIBOCISPEC_GENERATED_FILES "${LIBOCISPEC_DIR}/src/read-file.c")
    list(REMOVE_ITEM LIBOCISPEC_GENERATED_FILES "${LIBOCISPEC_GENERATED_DIR}/validate.c")

    # ------------------------------------------------------------------------------
    # Create a library for libocispec

    # Create a new library called libocispec from the generated files
    add_library(libocispec
        SHARED
        ${LIBOCISPEC_GENERATED_FILES}
    )

    target_include_directories(libocispec
        PUBLIC
        $<BUILD_INTERFACE:${LIBOCISPEC_GENERATED_DIR}>

        PRIVATE
        ${LIBOCISPEC_DIR}/src
        ${LIBOCISPEC_DIR} # Need to add this for the config.h generated by autotools
    )

    # Libocispec needs yajl to work
    target_link_libraries(libocispec
        yajl
    )

    install(
        TARGETS libocispec
        EXPORT DobbyTargets
        LIBRARY DESTINATION lib
    )

    set_target_properties(libocispec PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        SOVERSION 0
        PREFIX ""
    )

    install( DIRECTORY ${LIBOCISPEC_GENERATED_DIR}/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Dobby/rdkPlugins/
        FILES_MATCHING PATTERN "*.h"
    )
endfunction(GenerateLibocispec)


