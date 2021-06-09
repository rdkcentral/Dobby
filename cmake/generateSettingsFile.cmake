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

function(GenerateSettingsFile)
    cmake_parse_arguments(Argument "" "BASE_FILE;OUTPUT_FILE" "SETTINGS_APPEND_FILE" ${ARGN} )

    if(Argument_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "Unknown keywords given to GenerateSettingsFile(): \"${Argument_UNPARSED_ARGUMENTS}\"")
    endif()


    if(Argument_SETTINGS_APPEND_FILE)
        get_filename_component(BASE_FILE_ABS ${Argument_BASE_FILE} ABSOLUTE)
        message("Base settings file ${BASE_FILE_ABS}")
        # If we have file(s) to append to the base settings file, process them
        foreach(_file ${Argument_SETTINGS_APPEND_FILE})
            get_filename_component(_full_path ${_file} ABSOLUTE)
            message("Appending settings from ${_full_path}")
            list(APPEND appendfiles "${_full_path}")
        endforeach(_file)

        execute_process(
            WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
            COMMAND python3 ./daemon/process/settings/merge_settings.py --base ${BASE_FILE_ABS} --output ${Argument_OUTPUT_FILE} ${appendfiles}
            RESULT_VARIABLE ret
        )

        if (ret EQUAL "1")
            message(FATAL_ERROR "Failed to generate settings file")
        endif()
    endif()
endfunction(GenerateSettingsFile)
