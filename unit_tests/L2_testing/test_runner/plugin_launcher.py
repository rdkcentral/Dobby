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
import subprocess

container_name = "sleepy-thunder"
hook_name = "createRuntime"

tests = (
    test_utils.Test("Run plugin launcher",
                    container_name,
                    "Hook %s completed" % hook_name,
                    "Runs plugins launcher and check if it ran properly"),
)


def execute_test():

    output_table = []

    with test_utils.dobby_daemon(), test_utils.untar_bundle(container_name) as bundle_path:

        # createRuntime fails with the networking plugin without a container present with
        # permission issues. The networking plugin is not designed to work with standalone
        # DobbyPluginLauncher at the moment, so we can ignore the message. The test will
        # succeed because the networking plugin is set to required=false in the bundle config.

        # Test 0
        test = tests[0]
        command = ["DobbyPluginLauncher",
                   "-v",
                   "-h", hook_name,
                   "-c", bundle_path + "/config.json"]

        # cannot be simple run_command as we need input in this case
        # status = test_utils.run_command_line(command)

        # The state of the container MUST be passed to hooks over stdin so that they may
        # do work appropriate to the current state of the container.
        crun_input = """
        {
        "ociVersion": "1.0.2",
        "id": "%s",
        "status": "running",
        "pid":12345,
        "bundle": "%s",
        "annotations": {
            "myKey": "myValue"
            }
        }
        """ % (container_name, bundle_path)

        status = subprocess.run(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            input=crun_input,
                            universal_newlines=True)

        result = test.expected_output.lower() in status.stderr.lower()
        output = test_utils.create_simple_test_output(test, result, log_content=status.stderr)

        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
