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
from os.path import basename

tests = [
    test_utils.Test("Logging to file",
                    "filelogging",
                    ["hello world 1","hello world 2","hello world 10"],
                    "Prints hello world 10 times, output should be contained in the logfile"),
    test_utils.Test("No logging",
                    "nolog",
                    "",
                    "Starts a container without any logfile"),
]


def execute_test():
    if test_utils.selected_platform == test_utils.Platforms.no_selection:
        return test_utils.print_unsupported_platform(basename(__file__), test_utils.selected_platform)

    with test_utils.dobby_daemon():
        output_table = []

        for test in tests:
            result = test_container(test.container_id, test.expected_output)
            output = test_utils.create_simple_test_output(test, result[0], result[1])

            output_table.append(output)
            test_utils.print_single_result(output)


    return test_utils.count_print_results(output_table)


def test_container(container_id, expected_output):
    """Runs container and check if output contains expected output

    Parameters:
    container_id (string): name of container to run
    expected_output (string): output that should be provided by container

    Returns:
    (pass (bool), message (string)): Returns if expected output found and message

    """

    test_utils.print_log("Running %s container test" % container_id, test_utils.Severity.debug)

    with test_utils.untar_bundle(container_id) as bundle_path:
        launch_result = test_utils.launch_container(container_id, bundle_path)

    if launch_result:
        return validate_output_file(container_id, expected_output)

    return False, "Container did not launch successfully"


def validate_output_file(container_id, expected_output):
    """Helper function for finding if expected output is inside log of container

    Parameters:
    container_id (string): name of container to run
    expected_output (string): output that should be provided by container

    Returns:
    (pass (bool), message (string)): Returns if expected output found and message

    """

    log = test_utils.get_container_log(container_id)

    # If given a list of outputs to check, loop through and return false is one of them is not in the output
    if isinstance(expected_output, list):
        for text in expected_output:
            if text.lower() not in log.lower():
                return False, "Output file did not contain expected text"
        return True, "Test passed"

    # Otherwise we've been given a string, so just check that one string
    if expected_output.lower() in log.lower():
        return True, "Test passed"
    else:
        return False, "Output file did not contain expected text"


if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()
