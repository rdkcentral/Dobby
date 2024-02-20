# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2024 Sky UK
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
from pathlib import Path

tests = [
    test_utils.Test("Pid limit default",
                    "sleepy",
                    "256",
                    "Starts contaiern with no pid limit specified, checks if default pid limit is set for container"),
    test_utils.Test("Pid limit no override",
                    "sleepy_pid_limit",
                    "1000",
                    "Starts contaienr with pid limit specified in config.json, checks if that pid limit was not overriden"),
]


def execute_test():
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
        command = ["DobbyTool",
                "start",
                container_id,
                bundle_path]

        status = test_utils.run_command_line(command)
        if "started '" + container_id + "' container" not in status.stdout:
            return False, "Container did not launch successfully"
        
        return validate_pid_limit(container_id, expected_output)


def validate_pid_limit(container_id, expected_output):
    """Helper function for finding if containers pid limit is as expected

    Parameters:
    container_id (string): name of container to run
    expected_output (string): expected containers pid limit

    Returns:
    (pass (bool), message (string)): True if pid limit matches expected_output

    """

    pid_limit = 0

    # check pids.max present in containers pid cgroup
    path = Path("/sys/fs/cgroup/pids/" + container_id + "/pids.max")
    if not path.is_file():
        return False, "%s not found" % path.absolute()

    with open(path, 'r') as fh:
        pid_limit = fh.readline().strip()

    if expected_output == pid_limit:
        return True, "Test passed"
    else:
        return False, "Pid limit different then expected (expected: '%s', actual: '%s')" % (expected_output, pid_limit)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()
