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
    test_utils.Test("Check echo",
                    "echo",
                    "hello world",
                    "Prints \"Hello World\" to the console as a baseline functionality test"),
    test_utils.Test("Check mounts",
                    "mounts-vm",
                    "lorem ipsum",
                    "Mounts a sample text file into the container and attempts to read it"),
    test_utils.Test("Check environment variables",
                    "envvar",
                    "12345",
                    "Set environment variables"),
    ### Skip ram test, which is currently not working as intended due to file access issues. Likely related to lack of access to cgroups
    ### namespace in containers which used to work beforehand with crun.
    #test_utils.Test("Check ram",
    #                "ram",
    #                "2998272",
    #                "Creates a container a 3MB RAM limit. Checks this limit by reading the value of /sys/fs/cgroup/memory/memory.limit_in_bytes"),
    test_utils.Test("Check private network",
                    "wget-private",
                    "unable to resolve host address",
                    "With Private networking, the container should not be able to reach external webpages"),
    test_utils.Test("Check network",
                    "wget",
                    "HTTP request sent, awaiting response... 200 OK",
                    "With NAT networking, the container should be able to fetch webpages"),
]


def platform_dependent_modifications():
    """Changes test 1 based on selected platform

    Parameters:
    None: global test_utils.selected_platform decides what to do

    Returns:
    Nothing

    Raises:
    'Unknown platform' if platform not supported

    """

    if test_utils.selected_platform == test_utils.Platforms.xi_6:
        tests[1] = test_utils.Test("Check mounts",
                                   "mounts-xi6",
                                   "JENKINS_BUILD_NUMBER=",
                                   "Mounts a sample text file into the container and attempts to read it")
    elif test_utils.selected_platform == test_utils.Platforms.virtual_machine:
        # nothing to do, already selected proper one
        pass
    else:
        raise Exception('Unknown platform')


def execute_test():
    if test_utils.selected_platform == test_utils.Platforms.no_selection:
        return test_utils.print_unsupported_platform(basename(__file__), test_utils.selected_platform)

    platform_dependent_modifications()

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

    spec = test_utils.get_container_spec_path(container_id)

    launch_result = test_utils.launch_container(container_id, spec)

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
