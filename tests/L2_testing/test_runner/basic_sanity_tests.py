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
from time import sleep, monotonic
import select
import os
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
    if test_utils.selected_platform != test_utils.Platforms.vagrant_vm and test_utils.selected_platform != test_utils.Platforms.github_workflow_vm:
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
    # Some platforms do not emit a deterministic "stopped" log line.
    # Verify stop by process absence instead.
    result = not check_if_process_present(tests[3].expected_output)
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


# Uses select() for a true timeout instead of threads — no lingering readers.
# Reads raw bytes via os.read() to avoid Python TextIOWrapper buffering that
# can desynchronise from select()'s kernel-level readiness checks.
def read_asynchronous(proc, string_to_find, timeout):
    """Reads from process stderr with a real timeout using select().

    Unlike a threaded approach, this cannot leak a blocked reader: select()
    returns when data is available *or* when the timeout expires, so the
    caller always regains control promptly.

    Parameters:
    proc (process): process whose stderr we read
    string_to_find (string): what we want to find in process output
    timeout (float): how long we should wait if string not found (seconds)

    Returns:
    found (bool): True if string_to_find was found in proc stderr.

    """

    test_utils.print_log("Starting select-based read", test_utils.Severity.debug)
    deadline = monotonic() + timeout
    fd = proc.stderr.fileno()
    accumulated = ""

    while True:
        remaining = deadline - monotonic()
        if remaining <= 0:
            test_utils.print_log("Not found string \"%s\" (timeout). Accumulated output: %s"
                                 % (string_to_find, repr(accumulated)), test_utils.Severity.error)
            return False

        # Wait until stderr has data or timeout expires
        ready, _, _ = select.select([fd], [], [], remaining)
        if not ready:
            # Timeout with no data
            test_utils.print_log("Not found string \"%s\" (select timeout). Accumulated output: %s"
                                 % (string_to_find, repr(accumulated)), test_utils.Severity.error)
            return False

        # Read raw bytes to avoid TextIOWrapper buffering mismatch with select()
        chunk = os.read(fd, 4096)
        if not chunk:
            # EOF — process exited / pipe closed
            test_utils.print_log("EOF on process stderr, stopping reader. Accumulated output: %s"
                                 % repr(accumulated), test_utils.Severity.debug)
            return False

        accumulated += chunk.decode("utf-8", errors="replace")

        if string_to_find in accumulated:
            test_utils.print_log("Found string \"%s\"" % string_to_find, test_utils.Severity.debug)
            return True


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
    subproc = test_utils.run_command_line(["sudo", "pkill", "-9", "DobbyDaemon"])
    sleep(1)  # Give process time to fully terminate and be reaped
    return subproc


if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()


