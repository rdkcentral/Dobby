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
        self.container_id = container_id
        self.extract_root = get_bundle_path(container_id + "_bundle")
        self.path = self.extract_root
        self.valid = True

        print_log("untar'ing file %s.tar.gz" % self.path, Severity.debug)

        status = run_command_line(["tar",
                                   "-C",
                                   get_bundle_path(""),
                                   "-zxvf",
                                   self.path + ".tar.gz"])

        if status.returncode != 0:
            print_log("FATAL: Failed to extract bundle tarball '%s.tar.gz' (rc=%d): %s"
                      % (self.path, status.returncode, status.stderr.strip()),
                      Severity.error)
            self.valid = False
            return

        config_path = path.join(self.path, "config.json")
        if not path.exists(config_path):
            # It might be nested - tarball could contain "dirname/config.json"
            # Try to find it in the first level subdirectory
            try:
                import os
                entries = os.listdir(self.path)
                for entry in entries:
                    candidate = path.join(self.path, entry, "config.json")
                    if path.exists(candidate):
                        print_log("Found config.json nested in %s, updating path" % entry, Severity.debug)
                        self.path = path.join(self.path, entry)
                        config_path = candidate
                        break
            except Exception as err:
                print_log("Error checking nested bundle structure: %s" % err, Severity.warning)

        if not path.exists(config_path):
            print_log("FATAL: Extracted bundle is missing config.json. Expected at: %s" % config_path,
                      Severity.error)
            self.valid = False
            return

        # Patch config.json for cgroup v2 compatibility
        # cgroup v2: swappiness is NOT supported, strip it from config
        # cgroup v1: swappiness is supported, keep it (e.g., swappiness: 60)
        if is_cgroup_v2():
            patch_config_for_cgroupv2(config_path)

    def __enter__(self):
        """Returns the bundle path when valid, or None when extraction/validation
        failed.  Callers must check .valid (or the returned path) before use."""
        if not self.valid:
            return None
        return self.path

    def __exit__(self, etype, value, traceback):
        # Always clean up extraction root, even when runtime path was nested
        if path.exists(self.extract_root):
            print_log("Cleaning up bundle at: %s" % self.extract_root, Severity.debug)
            run_command_line(["rm",
                              "-rf",
                              self.extract_root])
        else:
            print_log("Bundle path doesn't exist, skipping cleanup: %s" % self.extract_root, Severity.debug)

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

        # Wait for D-Bus service registration (can be delayed on CI)
        for _ in range(20):
            probe = subprocess.run(["DobbyTool", "info", "__dobby_probe__"],
                                   stdout=subprocess.PIPE,
                                   stderr=subprocess.PIPE,
                                   universal_newlines=True)

            combined = (probe.stdout + probe.stderr).lower()

            # If daemon crashed/exited, stop waiting
            if self.subproc.poll() is not None:
                break

            # Service is ready once ServiceUnknown is gone (unknown container is fine)
            if "serviceunknown" not in combined and "org.rdk.dobby was not provided" not in combined:
                break

            sleep(0.25)

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


def is_cgroup_v2():
    """Detect if the system is using cgroup v2 (unified hierarchy).
    
    Check if we are in cgroup v2 by looking for cgroup.controllers file.
    
    Returns:
        bool: True if cgroup v2 is in use, False for cgroup v1
    """
    # Check if cgroup v2 is mounted at /sys/fs/cgroup
    return path.exists('/sys/fs/cgroup/cgroup.controllers')


def is_legacy_cgroup_available():
    """Check if legacy cgroup features (like swappiness) are available.
    
    cgroup v1: Uses memory.swappiness to control swap behavior
               e.g., echo 60 > /sys/fs/cgroup/memory/path/to/your/group/memory.swappiness
    cgroup v2: Uses memory.swap.max (0 = disable swap)
               There is no direct equivalent to 'swappiness' per-cgroup.
               e.g., echo 0 > /sys/fs/cgroup/path/to/your/group/memory.swap.max
    
    Returns:
        bool: True if legacy cgroup features are available (cgroup v1),
              False if using cgroup v2 without legacy features
    """
    if is_cgroup_v2():
        # cgroup v2: no swappiness support
        return False
    else:
        # cgroup v1: swappiness supported
        return True


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
        # If it's a bundle directory on cgroup v2, patch it
        if is_cgroup_v2():
            patch_config_for_cgroupv2(config_path)
    elif not path.exists(spec_path):
        print_log("Spec path does not exist: %s" % spec_path, Severity.error)
        return False
    else:
        # It's a spec file (not a bundle directory)
        # On cgroup v2, generate a bundle and patch it to remove swappiness
        if is_cgroup_v2():
            print_log("Detected cgroup v2 - generating patched bundle for %s" % container_id, Severity.debug)
            bundle_path = generate_bundle_from_spec(container_id)
            if bundle_path:
                spec_path = bundle_path
            else:
                print_log("Warning: Failed to generate bundle, using original spec (may fail on cgroup v2)", Severity.warning)

    # Use DobbyTool to launch container
    process = None
    output = ""
    combined_output = ""

    # Retry start when D-Bus registration races on CI
    for _ in range(3):
        process = run_command_line(["DobbyTool", "start", container_id, spec_path])
        output = process.stdout
        combined_output = (process.stdout + process.stderr).lower()

        if "started" in output:
            break

        if "serviceunknown" in combined_output or "org.rdk.dobby was not provided" in combined_output:
            sleep(0.5)
            continue

        break

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
    if process and process.stderr:
        print_log("DobbyTool start failed for %s: %s" % (container_id, process.stderr.strip()), Severity.error)

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


def generate_bundle_from_spec(container_id, output_dir=None):
    """Generate an OCI bundle from a Dobby spec file using DobbyBundleGenerator.
    
    Parameters:
    container_id (string): name of container (used to find spec file)
    output_dir (string): optional output directory for the bundle
    
    Returns:
    bundle_path (string): path to generated bundle, or None on failure
    """
    spec_path = get_container_spec_path(container_id)
    
    if output_dir is None:
        output_dir = get_bundle_path(container_id + "_generated")
    
    # Clean up existing bundle directory if it exists
    if path.exists(output_dir):
        print_log("Removing existing bundle directory: %s" % output_dir, Severity.debug)
        run_command_line(["rm", "-rf", output_dir])
    
    # Generate bundle using DobbyBundleGenerator
    result = run_command_line(["DobbyBundleGenerator", "-i", spec_path, "-o", output_dir])
    
    if result.returncode != 0 or "failed" in result.stderr.lower():
        print_log("Failed to generate bundle for %s: %s" % (container_id, result.stderr), Severity.error)
        return None
    
    config_path = path.join(output_dir, "config.json")
    if not path.exists(config_path):
        print_log("Generated bundle missing config.json: %s" % config_path, Severity.error)
        return None
    
    # Patch for cgroup v2 if needed
    if is_cgroup_v2():
        patch_config_for_cgroupv2(config_path)
    
    return output_dir


def fix_trailing_commas(json_str):
    """Fix trailing commas in JSON string that DobbyBundleGenerator may produce.
    
    Removes patterns like:
    - ,} -> }
    - ,] -> ]
    """
    import re
    # Keep applying until no more changes (handles nested cases)
    prev = None
    while prev != json_str:
        prev = json_str
        # Remove trailing commas before closing braces/brackets
        # Handle whitespace and newlines between comma and closing bracket
        json_str = re.sub(r',\s*}', '}', json_str)
        json_str = re.sub(r',\s*\]', ']', json_str)
    return json_str


def patch_config_for_cgroupv2(config_path):
    """Patch OCI config.json for cgroup v2 compatibility.
    
    cgroup v1: Uses memory.swappiness to control swap behavior
    cgroup v2: Uses memory.swap.max (no direct swappiness equivalent)
    
    Also handles other cgroup v2 incompatibilities like rootfsPropagation
    and null CPU realtime settings.
    """
    try:
        with open(config_path, 'r') as f:
            content = f.read()
        
        # Fix trailing commas that DobbyBundleGenerator may produce
        original_content = content
        content = fix_trailing_commas(content)
        
        if content != original_content:
            print_log("Fixed trailing commas in config.json", Severity.debug)
        
        try:
            config = json.loads(content)
        except json.JSONDecodeError as e:
            print_log("Warning: JSON parse error after fixing trailing commas: %s" % e, Severity.warning)
            print_log("Attempting to save fixed JSON and retry...", Severity.debug)
            # Write the fixed content back and try loading from file
            with open(config_path, 'w') as f:
                f.write(content)
            try:
                with open(config_path, 'r') as f:
                    config = json.load(f)
            except json.JSONDecodeError as e2:
                print_log("Warning: Still failed to parse JSON: %s" % e2, Severity.warning)
                return
        
        modified = False
        
        if 'linux' in config and 'resources' in config['linux']:
            resources = config['linux']['resources']
            
            # Remove swappiness from linux.resources.memory
            if 'memory' in resources and 'swappiness' in resources['memory']:
                del resources['memory']['swappiness']
                modified = True
                print_log("Stripped 'swappiness' from config.json for cgroup v2 compatibility", Severity.debug)
            
            # Fix cpu realtime settings - remove null values
            if 'cpu' in resources:
                cpu = resources['cpu']
                if cpu.get('realtimeRuntime') is None:
                    del cpu['realtimeRuntime']
                    modified = True
                    print_log("Removed null 'realtimeRuntime' for cgroup v2 compatibility", Severity.debug)
                if cpu.get('realtimePeriod') is None:
                    del cpu['realtimePeriod']
                    modified = True
                    print_log("Removed null 'realtimePeriod' for cgroup v2 compatibility", Severity.debug)
                # Remove cpu section entirely if empty
                if not cpu:
                    del resources['cpu']
                    modified = True
                    print_log("Removed empty 'cpu' section for cgroup v2 compatibility", Severity.debug)
        
        # Remove rootfsPropagation - causes "make rootfs private" errors
        # in user namespace environments like GitHub Actions
        if 'linux' in config and 'rootfsPropagation' in config['linux']:
            del config['linux']['rootfsPropagation']
            modified = True
            print_log("Removed linux.rootfsPropagation for cgroup v2 compatibility", Severity.debug)
        
        # Remove top-level rootfsPropagation as well
        if 'rootfsPropagation' in config:
            del config['rootfsPropagation']
            modified = True
            print_log("Removed top-level rootfsPropagation for cgroup v2 compatibility", Severity.debug)
        
        # Remove user namespace settings - causes issues in GitHub Actions 
        # which already uses user namespaces
        if 'linux' in config:
            # Remove uidMappings and gidMappings
            if 'uidMappings' in config['linux']:
                del config['linux']['uidMappings']
                modified = True
                print_log("Removed uidMappings for cgroup v2 compatibility", Severity.debug)
            if 'gidMappings' in config['linux']:
                del config['linux']['gidMappings']
                modified = True
                print_log("Removed gidMappings for cgroup v2 compatibility", Severity.debug)
            
            # Remove 'user' from namespaces list
            if 'namespaces' in config['linux']:
                namespaces = config['linux']['namespaces']
                original_len = len(namespaces)
                config['linux']['namespaces'] = [ns for ns in namespaces if ns.get('type') != 'user']
                if len(config['linux']['namespaces']) < original_len:
                    modified = True
                    print_log("Removed 'user' namespace for cgroup v2 compatibility", Severity.debug)
        
        if modified:
            with open(config_path, 'w') as f:
                json.dump(config, f, indent=4)
    except Exception as err:
        print_log("Warning: Failed to patch config.json for cgroup v2: %s" % err, Severity.warning)


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
        # On cgroup v2, generate bundle first and patch it to remove swappiness
        if is_cgroup_v2():
            bundle_path = generate_bundle_from_spec(container_id)
            if bundle_path:
                full_command.append(bundle_path)
            else:
                # Fallback to spec if bundle generation fails
                container_path = get_container_spec_path(container_id)
                full_command.append(container_path)
        else:
            container_path = get_container_spec_path(container_id)
            full_command.append(container_path)

    process = run_command_line(full_command)

    return process


