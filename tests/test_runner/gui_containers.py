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
from os import path, chdir
from collections import namedtuple
import subprocess
import signal
import json

# base fields - same as in test_utils.Test (except expected_output which is regular expression here)
# command - string, command to send in this test
Test = namedtuple('Test', ['name',
                           'container_id',
                           'expected_output',
                           'description',
                           'command']
                  )

tests = (
    Test("Run YouTube",
         "YouTube",
         "",
         "Runs \"YouTube\" application",
         "launch"),
    Test("Check YouTube running",
         "YouTube",
         "VISIBLE",
         "Check if \"YouTube\" application is running",
         "check"),
    Test("Close YouTube",
         "YouTube",
         "",
         "Closes \"YouTube\" application",
         "close"),
    Test("Check YouTube closed",
         "YouTube",
         "CLOSED",
         "Check if \"YouTube\" application was closed",
         "check"),
    Test("Run Xite",
         "com.sky.xite",
         "",
         "Runs \"Xite\" application",
         "launch"),
    Test("Check Xite running",
         "com.sky.xite",
         "VISIBLE",
         "Check if \"Xite\" application is running",
         "check"),
    Test("Close Xite",
         "com.sky.xite",
         "",
         "Closes \"Xite\" closed",
         "close"),
    Test("Check Xite running",
         "com.sky.xite",
         "CLOSED",
         "Check if \"Xite\" application was closed",
         "check"),
)


def create_curl_command(application, action):
    curl_command = ["curl",
                    "-X",
                    "POST",
                    "--url",
                    "http://127.0.0.1:9005/as/apps/action/%s?appId=%s" % (action, application),
                    "-d",
                    ""]
    return curl_command


def execute_test():
    if test_utils.selected_platform != test_utils.Platforms.xi_6:
        return test_utils.print_unsupported_platform(path.basename(__file__), test_utils.selected_platform)

    output_table = []

    # check if platform is supported
    if test_utils.selected_platform == test_utils.Platforms.virtual_machine:
        test_utils.print_unsupported_platform(path.basename(__file__), test_utils.selected_platform)
        return test_utils.count_print_results(output_table)

    for test in tests:
        if test.command == "check":
            success = check_app_status(test.container_id, test.expected_output)
            output = test_utils.create_simple_test_output(test, success)
        else:
            full_command = create_curl_command(test.container_id, test.command)
            result = test_utils.run_command_line(full_command)
            success = test.expected_output in result.stdout
            # success = search(test.expected_output, result.stdout) is not None
            output = test_utils.create_simple_test_output(test, success, log_content=result.stdout+result.stderr)

        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


def get_gui_test_path():
    """Gets path to gui test, which reads websocket. This is done so no external high-python-version library were
    required

    Parameters:
    None

    Returns:
    gui_path (path): path to JS gui test

    """
    js_test_name = "gui_test.js"

    # relative path "test_location/%s" % js_test_name"
    gui_path = path.abspath(path.join(path.dirname(__file__), js_test_name))
    test_utils.print_log("JS gui test path is %s" % gui_path, test_utils.Severity.debug)
    return gui_path


def get_websocket_data():
    """Runs gui_test.js inside Spark. Gets as/apps/status and closes spark.

    Parameters:
    None

    Returns:
    buffer (string): string that contains json with running apps

    """

    script_to_run = ["./Spark", get_gui_test_path()]
    starting_point = "-----------------------begin_as_apps_status-----------------------"
    end_point = "----------------------end_of_as_apps_status----------------------"

    original_path = path.abspath(path.dirname(__file__))
    chdir("/home/root/")

    subproc = subprocess.Popen(script_to_run,
                               universal_newlines=True,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE,
                               stdin=subprocess.PIPE)

    buffer = ""
    inside_block = False

    while True:
        line = subproc.stdout.readline()
        test_utils.print_log("Spark - %s" % line, test_utils.Severity.debug)

        if end_point in line:
            break

        if inside_block:
            buffer += line

        if starting_point in line:
            inside_block = True

    # send ctrl + c to end process
    subproc.send_signal(signal.SIGINT)

    chdir(original_path)

    return buffer


def check_app_status(app_id, status):
    """Check if app is in selected state

    Parameters:
    app_id (string): id of app we want to check
    status (string): state in which app should be

    Returns:
    found (bool): True if app found in selected state

    """

    input_string = get_websocket_data()
    applications = json.loads(input_string)

    found = False

    # as there is no status closed, there cannot be app with provided id
    if status == "CLOSED":
        found_ids = []
        for application in applications.get("apps"):
            found_id = application.get("appId")
            if found_id:
                found_ids.append(found_id)

        if app_id not in found_ids:
            found = True
    else:
        for application in applications.get("apps"):
            if application.get("appId") == app_id and application.get("status") == status:
                found = True
                break

    return found


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
