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

import test_utils

# in case we would like to change container name
container_name = "sleepy"

tests = (
    test_utils.Test("Create bundle",
                    container_name,
                    "Dobby Bundle Generator Tool",
                    "Generates bundle with DobbyBundleGenerator"),
    test_utils.Test("Diff bundles",
                    container_name,
                    "",
                    "Compares between original bundle and newly generated one"),
    test_utils.Test("Remove bundle",
                    container_name,
                    "",
                    "Removes files created by this test group"),
)


def execute_test():

    # this testcase is using tarball bundle so it gets all empty folders. They would get skipped by git. So in that
    # case it is better to untar and cleanup to have easy "diff" than to hack "diff -ignore" files.

    output_table = []

    with test_utils.untar_bundle(container_name) as bundle_path:
        # Test 0
        test = tests[0]
        status = test_utils.run_command_line(["DobbyBundleGenerator",
                                              "-i",
                                              test_utils.get_container_spec_path(test.container_id),
                                              "-o",
                                              test_utils.get_bundle_path(test.container_id)])

        message = ""
        result = True
        if test.expected_output.lower() not in status.stdout.lower():
            message = "Failed to run Dobby Bundle Generator Tool"
            result = False
        if "failed to create bundle" in status.stderr.lower():
            message = "Failed to create bundle"
            result = False

        output = test_utils.create_simple_test_output(test, result, message, status.stderr)
        output_table.append(output)
        test_utils.print_single_result(output)

        # Test 1
        test = tests[1]
        status = test_utils.run_command_line(["diff",
                                              "-r --ignore-space-change",
                                              test_utils.get_bundle_path(test.container_id),
                                              bundle_path])

        output = test_utils.create_simple_test_output(test, status.stdout is "", "", status.stdout)
        output_table.append(output)
        test_utils.print_single_result(output)

        # Test 2
        test = tests[2]
        status = test_utils.run_command_line(["rm",
                                              "-rf",
                                              test_utils.get_bundle_path(test.container_id)])

        output = test_utils.create_simple_test_output(test, status.stderr is "", "", status.stderr)
        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
