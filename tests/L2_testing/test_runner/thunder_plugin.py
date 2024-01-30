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
import subprocess
from time import sleep
from re import search
from os.path import basename

# base fields - same as in test_utils.Test (except expected_output which is regular expression here)
# command - string, command to send in this test
Test = namedtuple('Test', ['name',
                           'container_id',
                           'expected_output',
                           'description',
                           'command']
                  )

# in case we would like to change container name
container_name = "sleepy-thunder"


def create_successful_regex_answer(additional_content=""):
    expression = '{"jsonrpc":"2\\.0","id":3,"result":{%s"success":true}}' % additional_content
    test_utils.print_log('Regular expression is: @%s@' % expression, test_utils.Severity.debug)
    return expression


def create_tests():
    epg_running_re = ""
    if test_utils.selected_platform == test_utils.Platforms.xi_6:
        # does epg must ber running or can it run on xi6? assumed it can run but not must.
        epg_running_re = '({"Descriptor":\\d+,"Id":"com.bskyb.epgui"})?,?'

    tests = (
        Test("List no containers",
             container_name,
             create_successful_regex_answer('"containers":\\['+
                                            epg_running_re +
                                            '\\],'),
             "Sends request for listing all containers, should find none",
             "listContainers"),
        Test("Start bundle container",
             container_name,
             create_successful_regex_answer('"descriptor":\\d+,'),
             "Starts container using bundle",
             "startContainer"),
        Test("List running container %s" % container_name,
             container_name,
             create_successful_regex_answer('"containers":\\[' +
                                            epg_running_re +
                                            '{"Descriptor":\\d+,"Id":"%s"}\\],' % container_name),
             "Sends request for listing all containers, should find one",
             "listContainers"),
        Test("Pause container",
             container_name,
             create_successful_regex_answer(),
             "Sends pause request to container",
             "pauseContainer"),
        Test("Get state - paused",
             container_name,
             create_successful_regex_answer('"containerId":"%s","state":"Paused",' % container_name),
             "Send get container state request, should be paused",
             "getContainerState"),
        Test("Resume container",
             container_name,
             create_successful_regex_answer(),
             "Sends resume request to container",
             "resumeContainer"),
        Test("Get state - resumed",
             container_name,
             create_successful_regex_answer('"containerId":"%s","state":"Running",' % container_name),
             "Send get container state request, should be running again",
             "getContainerState"),
        Test("Stop container",
             container_name,
             create_successful_regex_answer(),
             "Stops container",
             "stopContainer"),
        Test("Start Dobby spec container",
             container_name,
             create_successful_regex_answer('"descriptor":\\d+,'),
             "Starts container using a Dobby spec",
             "startContainerFromDobbySpec"),
        Test("Stop container",
             container_name,
             create_successful_regex_answer(),
             "Stops container",
             "stopContainer"),
    )

    return tests


def create_curl_command(test, bundle_path):
    """Creates curl command for selected testcase

    Parameters:
    test (Test): test case for which to create curl

    Returns:
    curl_command (list(string)): command in format required by subprocess.run

    """

    params = ""
    if test.command != "listContainers":
        params += '"containerId": "%s"' % test.container_id
    if test.command == "startContainer":
        params += ', "bundlePath": "%s"' % bundle_path
    if test.command == "startContainerFromDobbySpec":
        with open(test_utils.get_container_spec_path("sleepy"), 'r') as file:
            data = file.read().replace('\n', '').replace(' ', '')
        params += ', "dobbySpec": %s' % data

    if params:
        params = ', "params":{%s}' % params

    plugin_name = "org.rdk.OCIContainer.1"

    if test_utils.selected_platform == test_utils.Platforms.xi_6:
        plugin_name = "org.rdk." + plugin_name

    curl_command = ["curl",
                    "--silent",
                    "-X",
                    "POST",
                    "http://127.0.0.1:9998/jsonrpc/",
                    "--header",
                    "Content-Type: application/json",
                    "-d",
                    '{ "jsonrpc": "2.0", "id": 3, "method": "%s.%s"%s}' % (plugin_name, test.command, params)]
    return curl_command


def start_wpeframework():
    """Starts wpeframework based on platform

    Parameters:
    None: global test_utils.selected_platform decides what to do

    Returns:
    subproc (process): process containing wpeframework, True if it is run in boot up

    Raises:
    'Unknown platform' if platform not supported

    """

    if test_utils.selected_platform == test_utils.Platforms.virtual_machine:
        return start_wpeframework_vm()
    elif test_utils.selected_platform == test_utils.Platforms.xi_6:
        # WPEFramework normally runs as a systemd service on Xi6 so doesn't need to be launched manually.
        return True
    else:
        raise Exception('Unknown platform')


def stop_wpeframework(process):
    """Stops wpeframework based on platform

    Parameters:
    process (process): process that should be stopped

    Returns:
    None

    Raises:
    'Unknown platform' if platform not supported

    """

    if test_utils.selected_platform == test_utils.Platforms.virtual_machine:
        stop_wpeframework_vm(process)
    elif test_utils.selected_platform == test_utils.Platforms.xi_6:
        # WPEFramework normally runs as a systemd service on Xi6 so doesn't need to be closed manually.
        pass
    else:
        raise Exception('Unknown platform')


def start_wpeframework_vm():
    """Starts wpeframework on virtual machine

    Parameters:
    None

    Returns:
    subproc (process): process containing wpeframework

    """

    test_utils.print_log("Starting WPEFramework", test_utils.Severity.debug)

    # as this process is running infinitely we cannot use run_command_line as it waits for execution to end
    subproc = subprocess.Popen(["/usr/bin/WPEFramework"],
                               universal_newlines=True,
                               stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    sleep(2)
    return subproc


def stop_wpeframework_vm(process):
    """Stops wpeframework on virtual machine

    Parameters:
    None

    Returns:
    None

    """

    process.communicate(input="quit\n")


def execute_test():
    if test_utils.selected_platform == test_utils.Platforms.no_selection:
        return test_utils.print_unsupported_platform(basename(__file__), test_utils.selected_platform)

    tests = create_tests()

    wpeframework = start_wpeframework()

    output_table = []

    with test_utils.dobby_daemon(), test_utils.untar_bundle(container_name) as bundle_path:
        for test in tests:
            full_command = create_curl_command(test, bundle_path)
            result = test_utils.run_command_line(full_command)
            # success = test.expected_output in result.stdout
            success = search(test.expected_output, result.stdout) is not None
            output = test_utils.create_simple_test_output(test, success, log_content=result.stdout+result.stderr)

            output_table.append(output)
            test_utils.print_single_result(output)

    stop_wpeframework(wpeframework)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()
