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

from os import path
import subprocess
from time import sleep
from enum import IntEnum
from collections import namedtuple
from argparse import ArgumentParser

Test = namedtuple('Test', ['name',
                           'container_id',
                           'expected_output',
                           'description']
                  )

TestResult = namedtuple('TestResult', ['name',
                                       'passed',
                                       'description',
                                       'message',
                                       'container_log_file']
                        )


class untar_bundle:
    """Context manager for working with tarball bundles"""
    def __init__(self, container_id):
        self.path = get_bundle_path(container_id + "_bundle")

        print_log("untar'ing file %s.tar.gz" % self.path, Severity.debug)
        run_command_line(["tar",
                          "-C",
                          get_bundle_path(""),
                          "-zxvf",
                          self.path + ".tar.gz"])

    def __enter__(self):
        return self.path

    def __exit__(self, etype, value, traceback):
        print_log("deleting folder %s" % self.path, Severity.debug)
        run_command_line(["rm",
                          "-rf",
                          self.path])

class dobby_daemon:
    """Starts and stops DobbyDaemon service."""
    def __init__(self):
        if selected_platform == Platforms.xi_6:
            subprocess.run(["systemctl", "stop", "dobby"])
        else:
            subprocess.run(["sudo", "pkill", "DobbyDaemon"])
        sleep(1) # give DobbyDaemon time to terminate

        print_log("Starting Dobby Daemon (logging to Journal)...", Severity.debug)

        # as this process is running infinitely we cannot use run_command_line as it waits for execution to end
        self.subproc = subprocess.Popen(["sudo",
                                         "DobbyDaemon",
                                         "--nofork",
                                         "--journald",
                                         "--noconsole"
                                        ],
                                        universal_newlines=True,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)
        sleep(1) # give DobbyDaemon time to initialise

    def __enter__(self):
        return self.subproc

    def __exit__(self, etype, value, traceback):
        print_log("Stopping Dobby Daemon", Severity.debug)

        if selected_platform == Platforms.xi_6:
            self.subproc.kill()
        else:
            subprocess.run(["sudo", "pkill", "DobbyDaemon"])
        sleep(0.2)

        # check for segfault
        self.subproc.communicate()
        if self.subproc.returncode == -11: # -11 == SIGSEGV
            print_log("Received SIGSEGV from DobbyDaemon", Severity.error)

        # restart dobby service on xi6
        if selected_platform == Platforms.xi_6:
            subprocess.run(["systemctl", "start", "dobby"])


class Platforms(IntEnum):
    no_selection = 0
    virtual_machine = 1
    xi_6 = 2


class Severity(IntEnum):
    no_log = 0
    error = 1
    warning = 2
    info = 3
    debug = 4
    log_all = 5


# global fields that contains status of test runner
current_log_level = Severity.info
selected_platform = Platforms.no_selection


def print_log(log_message, log_severity):
    """Function that prints log only if severity is equal or higher than globally selected one

    Parameters:
    log_message (string): Log message that should be displayed
    log_severity (Severity): Severity of provided message

    Returns:
    None

    """

    if log_severity <= current_log_level:
        print(log_message)


def print_single_result(result):
    """Function that prints result of single test case. Passing results are severity debug, and failing are severity
    error. This means that if user has selected severity lower than debug he will not see passing results.

    Parameters:
    result (TestResult): Result of test case that should be displayed

    Returns:
    None

    """

    temp_severity = Severity.debug

    if result.passed is not True:
        temp_severity = Severity.error
        print_log("\n---------------Test Failed---------------", temp_severity)
    else:
        print_log("\n---------------Test Passed---------------", temp_severity)

    print_log("Name = %s" % result.name, temp_severity)
    print_log("Passed = %s" % result.passed, temp_severity)
    print_log("Description = %s" % result.description, temp_severity)
    print_log("Message = %s" % result.message, temp_severity)
    print_log("Log = %s" % result.container_log_file, temp_severity)
    print_log("---------------End Report---------------\n", temp_severity)


def print_unsupported_platform(test_name, platform):
    """Function that prints message that platform is not supported for selected test

    Parameters:
    test_name (string): Name of the test case
    platform (Platforms): Selected platform

    Returns:
    (0, 0): 0 successful tests, 0 tested

    """

    platform_names = []
    for name in Platforms:
        platform_names.append(str(name).replace("Platforms.", ""))

    print_log("Test '%s' is not supported for platform '%s', no tests executed"
              % (test_name, platform_names[platform]), Severity.warning)

    # return 0 successful, 0 tested
    return 0, 0


def count_print_results(output_table):
    """Helper function which counts and then prints results. This operation is so common that this function was required

    Parameters:
    output_table (list(TestResult)): List of test results to be counted and printed

    Returns:
    (success_count, test_count): How many test were passed, how many was taken

    """

    success_count, test_count = count_successes(output_table)
    return print_results(success_count, test_count)


def count_successes(output_table):
    """Counts how many tests in table was successful, and how many was tested

    Parameters:
    output_table (list(TestResult)): List of test results to be counted

    Returns:
    (success_count, test_count): How many test were passed, how many was taken

    """

    success_count = sum(output.passed is True for output in output_table)
    test_count = len(output_table)
    return success_count, test_count


def print_results(success_count, test_count):
    """Prints user how many tests were passed/taken

    Parameters:
    output_table (list(TestResult)): List of test results to be printed

    Returns:
    (success_count, test_count): How many test were passed, how many was taken

    """

    if success_count == test_count:
        print_log("All %d succeed" % test_count, Severity.info)
    else:
        print_log("%d out of %d succeed" % (success_count, test_count), Severity.info)
    return success_count, test_count


def create_simple_test_output(test, result, message="", log_content=""):
    """Creates object of type TestResults with basic body

    Parameters:
    test (Test [or other that have at least all fields of Test]): description of test taken
    result (bool): if test was successful
    [message]: optional short description why testcase was unsuccessful/successful, if not provided simple
        "Test passed/failed" will be displayed
    [log_content]: optional log with debug information so user can dig out reason of fail

    Returns:
    output (TestResult): Result of test case in proper format

    """

    if log_content == "":
        log_content = get_container_log(test.container_id)

    if result:
        if message == "":
            message = "Test passed"

        output = TestResult(test.name,
                            True,
                            test.description,
                            message,
                            log_content)
    else:
        if message == "":
            message = "Test failed"

        output = TestResult(test.name,
                            False,
                            test.description,
                            message,
                            log_content)
    return output


def command_to_string(command):
    """
    Changes command from table of string (format accepted by subprocess.run) to string that user can copy-paste to
    terminal to debug
    :param command: command in format accepted by subprocess.run (table of strings)
    :return: string that can be copy-pasted to terminal
    """
    string_command = ""
    for subcommand in command:
        if " " in subcommand:
            string_command += " '%s'" % subcommand
        else:
            string_command += " %s" % subcommand
    if string_command:
        # remove leading space
        string_command = string_command[1:]
    return string_command


def run_command_line(command):
    """Runs command in command line, waits for execution to end

    Parameters:
    command (list(string)): list of commands, for parameters each parameter must be in separate list element i.e.
        ['ls', '-la']. It will NOT work if you simply use ['ls -la']

    Returns:
    status (subprocess.run): Instance of run on which user can check stdout/stderr

    """

    print_log("Running command line command = \"%s\"" % command_to_string(command), Severity.debug)

    status = subprocess.run(command,
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE,
                            universal_newlines=True)

    return status


def launch_container(container_id, spec_path):
    """Starts container using DobbyTool

    Parameters:
    container_id (string): name of container to be created
    spec_path (path): path to spec file which contains desired container

    Returns:
    status (bool): True if container created, False if there was some error

    """

    print_log("Launching container %s with spec %s" % (container_id, spec_path), Severity.debug)

    # Use DobbyTool to launch container
    process = run_command_line(["DobbyTool", "start", container_id, spec_path])
    output = process.stdout

    # Check DobbyTool has started the container
    if "started" in output:
        # Wait for container to exit (max wait of 5 seconds)
        print_log("Container launched, waiting for exit...", Severity.debug)

        for i in range(0, 10):
            sleep(0.5)
            status = run_command_line(["DobbyTool", "info", container_id])

            if "failed to find" in status.stdout:
                # Container has finished
                print_log("Container has exited", Severity.debug)
                return True

        # Timeout
        print_log("Waited 5 seconds for exit.. timeout", Severity.error)
        return True
    return False


def get_container_log(container_id):
    """Gets content of log file generated for container based on its id

    Parameters:
    container_id (string): name of container for which log we search

    Returns:
    data (string): Log for container, empty string if container log not found

    """

    log_path = "/tmp/%s.log" % container_id

    if path.exists(log_path):
        with open(log_path) as f:
            data = f.read()
        return data
    return ""


def get_container_spec_path(container_id):
    """Gets path to specification for container based on container id

    Parameters:
    container_id (string): name of container

    Returns:
    spec_path (path): path to specification of the container

    """

    # relative path "test_location/../dobby_specs/%s.json" % container_id"
    spec_path = path.abspath(path.join(path.dirname(__file__), '..', 'dobby_specs', '%s.json' % container_id))
    print_log("Container spec path is %s" % spec_path, Severity.debug)
    return spec_path


def get_bundle_path(container_id):
    """Gets path to bundle for container based on container id

    Parameters:
    container_id (string): name of container

    Returns:
    bundle_path (path): path to bundle of the container

    """

    # relative path "test_location/bundle/%s.json" % container_id"
    bundle_path = path.abspath(path.join(path.dirname(__file__), 'bundle', container_id))
    print_log("Bundle path is %s" % bundle_path, Severity.debug)
    return bundle_path


def parse_arguments(file_name, platform_required=False):
    """Sets global flags current_log_level and selected_platform based on user input.

    Parameters:
    file_name (string): name of test
    [platform_required] (bool): if True then platform parameter becomes required

    Returns:
    None

    """

    global current_log_level
    global selected_platform

    parser = ArgumentParser(prog='%s' % file_name,
                            usage='%s [options]' % file_name,
                            description='Performing test case for containers')

    possible_verbosity = ''
    for index, value in enumerate(Severity):
        possible_verbosity += "%d = %s, " % (index, str(value).replace('Severity.', ''))

    parser.add_argument('-v',
                        '--verbosity',
                        type=int,
                        choices=list(range(max(Severity) + 1)),
                        # delete last space and comma from string
                        help='Current test verbosity, possible options are:\n' + possible_verbosity[:-2])

    possible_platforms = ''
    for index, value in enumerate(Platforms):
        possible_platforms += "%d = %s, " % (index, str(value).replace('Platforms.', ''))

    parser.add_argument('-p',
                        '--platform',
                        type=int,
                        required=platform_required,
                        choices=list(range(max(Platforms) + 1)),
                        help='Platform on which tests are performed, some tests can be platform specific, '
                             # delete last space and comma from string
                             'possible options are:\n' + possible_platforms[:-2])

    args = parser.parse_args()

    # do not use simple 'if args.verbosity:' as 0 is valid option
    if args.verbosity is not None:
        current_log_level = args.verbosity
        print_log("Current log level set to %d" % current_log_level, Severity.debug)

    # do not use simple 'if args.platform:' as 0 is valid option
    if args.platform is not None:
        selected_platform = args.platform
        print_log("Current platform set to %d" % selected_platform, Severity.debug)
