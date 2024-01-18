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
from collections import namedtuple

# base fields - same as in test_utils.Test
# negation - Bool, if true instead finding expected_output we CANNOT find it
# command - string, command to send in this test
Test = namedtuple('Test', ['name',
                           'container_id',
                           'expected_output',
                           'description',
                           'negation',
                           'command']
                  )

# in case we would like to change container name
container_name = "sleepy"

tests = (
    Test("Check container not exists",
         container_name,
         "failed to find container",
         "Check if container is not running before tests",
         False,
         "info"),
    Test("Start container",
         container_name,
         "started '%s' container" % container_name,
         "Starts container and check if it was started",
         False,
         "start"),
    Test("Check container running",
         container_name,
         '"state" : "running"',
         "Check if container is running",
         False,
         "info"),
    Test("Pause container",
         container_name,
         "paused container '%s'" % container_name,
         "Pauses container and check if it was paused",
         False,
         "pause"),
    # there is no '"state":"paused"', I am not sure if this is bug of feature so for now I am passing it
    Test("Check container not running",
         container_name,
         '"state":"running"',
         "Check if container is not running",
         True,
         "info"),
    Test("Resume container",
         container_name,
         "resumed container '%s'" % container_name,
         "Resumes container and check if it was resumed",
         False,
         "resume"),
    Test("Check container running",
         container_name,
         '"state" : "running"',
         "Check if container is running",
         False,
         "info"),
    Test("Stop container",
         container_name,
         "stopped container '%s'" % container_name,
         "Stops container and check if it was stopped",
         False,
         "stop"),
)


def execute_test():
    output_table = []

    with test_utils.dobby_daemon():
        for test in tests:
            process = dobby_tool_command(test.command, test.container_id)
            test_utils.print_log("command output = %s" % process.stdout, test_utils.Severity.debug)
            result = test.expected_output in process.stdout
            if test.negation:
                result = not result
            output = test_utils.create_simple_test_output(test, result)
            output_table.append(output)
            test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


def dobby_tool_command(command, container_id):
    """Runs DobbyTool command

    Parameters:
    command (string): command that should be run
    container_id (string): name of container to run

    Returns:
    process (process): process that runs selected command

    """

    full_command = [
            "DobbyTool",
            command,
            container_id
        ]
    if command == "start":
        container_path = test_utils.get_container_spec_path(container_id)
        full_command.append(container_path)

    process = test_utils.run_command_line(full_command)

    return process


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
