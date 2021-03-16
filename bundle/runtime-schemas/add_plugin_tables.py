#
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
#

###############################################################################################
#
# Adds new rdk plugin fields for Dobby to be able to iterate through plugins and pass the data
# to the respective plugins.
#
# author: Marcin Koczwara
#
###############################################################################################

import re
from os import listdir

# Improvement idea - now we are allocating statically memory of found_plugins_count x longest_plugin_name for
# variable names_of_plugins. We could instead dynamically allocate memory of proper size (as we know it in
# "add_c_content". But using that would mean that we need to free that memory. This should be done inside
# "free_[a-z_]+_rdk_plugins". We would need to do it in loop using "ret->plugins_count" as amount of active
# plugins. This would improve memory usage when more plugins will be implemented.


def add_c_content(variable, file_content, line_number):
    file_content.insert(line_number, '    else if (ret->%s != NULL)\n' % variable)
    file_content.insert(line_number + 1, '      {\n')
    file_content.insert(line_number + 2, '        strcpy(ret->names_of_plugins[ret->plugins_count], "%s");\n' % variable)
    file_content.insert(line_number + 3, '        ret->required_plugins[ret->plugins_count] = ret->%s->required;\n' % variable)
    file_content.insert(line_number + 4, '        ret->plugins_count++;\n')
    file_content.insert(line_number + 5, '      }\n')


def add_h_content(found_plugins_count, longest_plugin_name, file_content, line_number):
    file_content.insert(line_number, '    bool required_plugins[%d];\n' % found_plugins_count)
    file_content.insert(line_number + 1, '\n')
    file_content.insert(line_number + 2, '    char names_of_plugins[%d][%d];\n' % (found_plugins_count, longest_plugin_name + 1))
    file_content.insert(line_number + 3, '\n')
    file_content.insert(line_number + 4, '    size_t plugins_count;\n')
    file_content.insert(line_number + 5, '\n')


def process_files():
    files = find_files()
    found_plugins_count, longest_plugin_name = process_c_file(files[0])
    process_h_file(files[1], found_plugins_count, longest_plugin_name)
    print("Successfully added")


def process_c_file(c_file):
    pattern = re.compile("if \(ret->([a-z]+) == NULL && \*err != 0\)")
    pattern2 = re.compile("ret = calloc \(1, sizeof \(\*ret\)\);")
    function_start_pattern = re.compile("make_[a-z_]+_rdk_plugins")
    function_end_pattern = re.compile("if \(tree->type == yajl_t_object && \(ctx->options & OPT_PARSE_STRICT\)\)")

    with open(c_file, 'r') as file:
        file_content = file.readlines()

    found_plugins_count = 0
    longest_plugin_name = 0

    inside_function = False

    for i, line in enumerate(file_content):
        if re.search(function_start_pattern, line):
            inside_function = True

        if inside_function:
            for match in re.finditer(pattern, line):
                # print('Found on line %s: %s' % (i+1, match.group()))
                # print(match.group(1))
                add_c_content(match.group(1), file_content, i+5)
                found_plugins_count += 1
                longest_plugin_name = max(longest_plugin_name, len(match.group(1)))

            if re.search(pattern2, line):
                # print('Found on line %s: %s' % (i + 1, match.group()))
                file_content.insert(i+3, "    ret->plugins_count = 0;\n")

            if re.search(function_end_pattern, line):
                # print("break at line %d" % i)
                break

    with open(c_file, 'w') as file:
        file.writelines(file_content)

    return found_plugins_count, longest_plugin_name


def process_h_file(h_file, found_plugins_count, longest_plugin_name):
    pattern = re.compile(".*rt_dobby_schema_rdk_plugins;")

    with open(h_file, 'r') as file:
        file_content = file.readlines()

    found = False

    for i, line in enumerate(file_content):
        if found:
            break
        if re.search(pattern, line):
            # print('Found on line %s: %s' % (i+1, match.group()))
            add_h_content(found_plugins_count, longest_plugin_name, file_content, i-1)
            found = True

    with open(h_file, 'w') as file:
        file.writelines(file_content)


def find_files():
    files = listdir()
    proper_files = []
    common_file_part = "rt_dobby_schema"

    for file in files:
        if "%s.c" % common_file_part in file \
                or "%s.h" % common_file_part in file:
            proper_files.append(file)

    if len(proper_files) != 2:
        print("Found wrong amount of files %d, should be 2" % len(proper_files))
        raise IndexError

    # should return *.c, *.h in this order
    if ".c" in proper_files[1]:
        proper_files[0], proper_files[1] = proper_files[1], proper_files[0]

    print("Found files:")
    print(proper_files)

    return proper_files


if __name__ == "__main__":
    process_files()
