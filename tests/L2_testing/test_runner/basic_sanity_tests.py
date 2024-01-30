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
from subprocess import check_output
import subprocess
from time import sleep
import multiprocessing
from os.path import basename

tests = (
    test_utils.Test("Start DobbyDaemon",
                    "No container",
                    "started Dobby daemon",
                    "Starts \"DobbyDaemon\" and check if it opened successfully"),
    test_utils.Test("Check DobbyDaemon running",
                    "No container",
                    "sudo DobbyDaemon --nofork --journald",
                    "Check if DobbyDaemon process is running"),
    test_utils.Test("Stop DobbyDaemon",
                    "No container",
                    "stopped Dobby daemon",
                    "Stops \"DobbyDaemon\" and check if it stopped successfully"),
    test_utils.Test("Check DobbyDaemon stopped",
                    "No container",
                    "sudo DobbyDaemon --nofork --journald",
                    "Check if DobbyDaemon process was killed"),

)


def execute_test():
    if test_utils.selected_platform != test_utils.Platforms.virtual_machine:
        return test_utils.print_unsupported_platform(basename(__file__), test_utils.selected_platform)

    output_table = []

    #TODO: Update to use test_utils.dobby_daemon class for consistency

    # Test 0
    test = tests[0]
    subproc = start_dobby_daemon()
    result = read_asynchronous(subproc, test.expected_output, 5)
    output = test_utils.create_simple_test_output(test, result)
    output_table.append(output)
    test_utils.print_single_result(output)

    # Test 1
    test = tests[1]
    result = check_if_process_present(test.expected_output)
    output = test_utils.create_simple_test_output(test, result)
    output_table.append(output)
    test_utils.print_single_result(output)

    # Test 2
    test = tests[2]
    stop_dobby_daemon()
    result = read_asynchronous(subproc, test.expected_output, 5)
    output = test_utils.create_simple_test_output(test, result)
    output_table.append(output)
    test_utils.print_single_result(output)

    # Test 3
    test = tests[3]
    # in this test we CANNOT find running DobbyDaemon
    result = not check_if_process_present(test.expected_output)
    output = test_utils.create_simple_test_output(test, result)
    output_table.append(output)
    test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


# we need to do this asynchronous as if there is no such string we would end in endless loop
def read_asynchronous(proc, string_to_find, timeout):
    """Reads asynchronous from process. Ends when found string or timeout occurred.

    Parameters:
    proc (process): process in which we want to read
    string_to_find (string): what we want to find in process
    timeout (float): how long we should wait if string not found (seconds)

    Returns:
    found (bool): True if found string_to_find inside proc.

    """

    # as this function should not be used outside asynchronous read, it is moved inside it
    def wait_for_string(proc, string_to_find):
        """Waits indefinitely until string is found in process. Must be run with timeout multiprocess.

        Parameters:
        proc (process): process in which we want to read
        string_to_find (string): what we want to find in process

        Returns:
        None: Returns nothing if found, never ends if not found

        """

        while True:
            # notice that all data are in stderr not in stdout, this is DobbyDaemon design
            output = proc.stderr.readline()
            if string_to_find in output:
                test_utils.print_log("Found string \"%s\"" % string_to_find, test_utils.Severity.debug)
                return

    found = False
    reader = multiprocessing.Process(target=wait_for_string, args=(proc, string_to_find), kwargs={})
    test_utils.print_log("Starting multithread read", test_utils.Severity.debug)
    reader.start()
    reader.join(timeout)
    # if thread still running
    if reader.is_alive():
        test_utils.print_log("Reader still exists, closing", test_utils.Severity.debug)
        reader.terminate()
        test_utils.print_log("Not found string \"%s\"" % string_to_find, test_utils.Severity.error)
    else:
        found = True
    return found


def check_if_process_present(string_to_find):
    """Checks if process runs on machine

    Parameters:
    string_to_find (string): process we want to find

    Returns:
    found (bool): True if found process running

    """

    output = check_output(["ps", "-ax"], universal_newlines=True)
    if string_to_find in output:
        return True
    else:
        return False


def start_dobby_daemon():
    """Starts DobbyDaemon service, this service is then run in background and must be closed with stop_dobby_daemon.

    Parameters:
    None

    Returns:
    subproc (subprocess.Popen): DobbyDaemon service process, True for platform xi_6

    """
    if test_utils.selected_platform == test_utils.Platforms.xi_6:
        subprocess.run(["systemctl", "stop", "dobby"])
    else:
        subprocess.run(["sudo", "pkill", "DobbyDaemon"])

    test_utils.print_log("Starting Dobby Daemon (logging to Journal)...", test_utils.Severity.debug)

    # as this process is running infinitely we cannot use run_command_line as it waits for execution to end
    subproc = subprocess.Popen(["sudo",
                                "DobbyDaemon",
                                "--nofork",
                                #"--noconsole",
                                "--journald"
                                ],
                               universal_newlines=True,
                               stdout=subprocess.PIPE,
                               stderr=subprocess.PIPE)
    sleep(1)
    return subproc


def stop_dobby_daemon():
    """Stops DobbyDaemon service. For Xi6 DobbyDaemon is running all the time so this function returns does nothing

    Parameters:
    None

    Returns:
    subproc (subprocess.run): killing DobbyDaemon process, True for platform xi_6

    """

    test_utils.print_log("Stopping Dobby Daemon", test_utils.Severity.debug)
    subproc = test_utils.run_command_line(["sudo", "pkill", "DobbyDaemon"])
    sleep(0.2)
    return subproc


if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()
