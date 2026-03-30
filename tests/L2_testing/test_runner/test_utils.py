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
import json

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
        import os
        self.container_id = container_id
        self.path = get_bundle_path(container_id + "_bundle")
        self.valid = True
        self.parent_path = get_bundle_path("")  # Save parent for cleanup
        self.actual_bundle_dir = None  # Track which directory we actually use

        print_log("untar'ing file %s.tar.gz" % self.path, Severity.debug)
        
        # Extract to parent directory
        status = run_command_line(["tar",
                                   "-C",
                                   self.parent_path,
                                   "-zxvf",
                                   self.path + ".tar.gz"])

        if status.returncode != 0:
            print_log("FATAL: Failed to extract bundle tarball '%s.tar.gz' (rc=%d): %s"
                      % (self.path, status.returncode, status.stderr.strip()),
                      Severity.error)
            self.valid = False
            return

        # Try to find config.json - it could be in multiple possible locations
        candidates = [
            # 1. At expected location: /path/filelogging_bundle/config.json
            (self.path, "expected location"),
            # 2. In a subdirectory: /path/filelogging_bundle/filelogging_bundle/config.json
            (path.join(self.path, container_id + "_bundle"), "self-named subdirectory"),
        ]
        
        # Also check for any immediate subdirectories
        if path.exists(self.path):
            try:
                entries = os.listdir(self.path)
                for entry in entries:
                    entry_path = path.join(self.path, entry)
                    if path.isdir(entry_path):
                        candidates.append((entry_path, "subdirectory '%s'" % entry))
            except Exception as err:
                print_log("Error listing bundle directory: %s" % err, Severity.warning)
        
        # Find first candidate with config.json
        config_found = False
        for candidate_path, location_desc in candidates:
            config_path = path.join(candidate_path, "config.json")
            print_log("Checking %s: %s" % (location_desc, config_path), Severity.debug)
            
            if path.exists(config_path):
                print_log("Found config.json in %s" % location_desc, Severity.info)
                self.path = candidate_path
                self.actual_bundle_dir = candidate_path
                config_found = True
                break
        
        if not config_found:
            # Final diagnostic: list what we actually extracted
            print_log("FATAL: Could not find config.json in bundle", Severity.error)
            if path.exists(self.path):
                try:
                    entries = os.listdir(self.path)
                    print_log("Directory contents of %s: %s" % (self.path, entries), Severity.error)
                    # Recursively list structure up to 2 levels
                    for entry in entries:
                        entry_path = path.join(self.path, entry)
                        if path.isdir(entry_path):
                            try:
                                sub_entries = os.listdir(entry_path)
                                print_log("  /%s/: %s" % (entry, sub_entries), Severity.error)
                            except:
                                pass
                except Exception as err:
                    print_log("Could not list bundle directory: %s" % err, Severity.error)
            self.valid = False
        else:
            print_log("Bundle validation successful at: %s" % self.path, Severity.debug)

    def __enter__(self):
        return self.path

    def __exit__(self, etype, value, traceback):
        # Clean up the actual directory we found, not the expected one
        # Only clean up if path exists (extraction might have failed)
        if path.exists(self.path):
            print_log("Cleaning up bundle at: %s" % self.path, Severity.debug)
            run_command_line(["rm",
                              "-rf",
                              self.path])
        else:
            print_log("Bundle path doesn't exist, skipping cleanup: %s" % self.path, Severity.debug)

class dobby_daemon:
    """Starts and stops DobbyDaemon service."""
    def __init__(self, log_to_stdout = False):
        """
        Params:
        log_to_stdout: if True Dobby logs will be visible together with test logs. Useful when debugging tests.
        """
        if selected_platform == Platforms.xi_6:
            subprocess.run(["systemctl", "stop", "dobby"])
        else:
            subprocess.run(["sudo", "pkill", "DobbyDaemon"])
        sleep(1) # give DobbyDaemon time to terminate

        print_log("Starting Dobby Daemon (logging to Journal)...", Severity.debug)

        if log_to_stdout:
            cmd = ["sudo", "DobbyDaemon", "--nofork"]
            kvargs = {"universal_newlines": True}
        else:
            cmd = ["sudo", "DobbyDaemon", "--nofork", "--journald", "--noconsole"]
            kvargs = {"universal_newlines": True, "stdout": subprocess.PIPE, "stderr": subprocess.PIPE}

        # as this process is running infinitely we cannot use run_command_line as it waits for execution to end
        self.subproc = subprocess.Popen(cmd, **kvargs)
        sleep(1) # give DobbyDaemon time to initialise

    def __enter__(self):
        return self.subproc

    def __exit__(self, etype, value, traceback):
        print_log("Stopping Dobby Daemon", Severity.debug)

        if selected_platform == Platforms.xi_6:
            self.subproc.kill()
        else:
            subprocess.run(["sudo", "pkill", "-9", "DobbyDaemon"])
        sleep(1)  # Give process time to fully terminate and be reaped

        # check for segfault
        try:
            self.subproc.communicate(timeout=2)
        except subprocess.TimeoutExpired:
            self.subproc.kill()
            self.subproc.wait()
        
        if self.subproc.returncode == -11: # -11 == SIGSEGV
            print_log("Received SIGSEGV from DobbyDaemon", Severity.error)

        # restart dobby service on xi6
        if selected_platform == Platforms.xi_6:
            subprocess.run(["systemctl", "start", "dobby"])


class Platforms(IntEnum):
    no_selection = 0
    vagrant_vm = 1
    xi_6 = 2
    github_workflow_vm = 3


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
        print(log_message, flush=True)


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

    # Convert named tuples to dictionaries
    test_results_dict_list = [{key: value for key, value in result._asdict().items() if key != 'container_log_file'} for result in output_table]

    # Save the data to a JSON file
    json_file_path = 'test_results.json'
    with open(json_file_path, 'w') as json_file:
        json.dump(test_results_dict_list, json_file, indent=2)

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

    # Validate input path early for clearer errors.
    if path.isdir(spec_path):
        config_path = path.join(spec_path, "config.json")
        if not path.exists(config_path):
            print_log("Bundle path missing config.json: %s" % config_path, Severity.error)
            return False
    elif not path.exists(spec_path):
        print_log("Spec path does not exist: %s" % spec_path, Severity.error)
        return False

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


def dobby_tool_command(command, container_id, params=None):
    """Runs DobbyTool command

    Parameters:
    command (string): command that should be run
    container_id (string): name of container to run
    [params] (list(string)): additional parameters that should be passed to command

    Returns:
    process (process): process that runs selected command

    """

    full_command = ["DobbyTool", command]
    
    if params:
        full_command.extend(params)
    
    full_command.append(container_id)

    if command == "start":
        container_path = get_container_spec_path(container_id)
        full_command.append(container_path)

    process = run_command_line(full_command)

    return process

