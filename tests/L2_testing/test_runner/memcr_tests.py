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
import subprocess
import signal
import json
from time import sleep
from collections import namedtuple
from pathlib import Path

# base fields - same as in test_utils.Test
# test_func - string, function name that performs the test
Test = namedtuple('Test', ['name',
                           'container_id',
                           'expected_output',
                           'description',
                           'test_func']
                  )

tests = (
    Test("Basic memcr test",
         "sleepy",
         True,
         "Starts container, hibernates it and wakes it up",
         "basic_memcr_test"),
)


class memcr:
    """ Starts memcr """
    def __init__(self):
        test_utils.print_log("Starting memcr", test_utils.Severity.debug)
        # as this process is running infinitely we cannot use run_command_line as it waits for execution to end
        self.subproc = subprocess.Popen(["~/memcr/scripts/start_memcr.sh"],
                                        universal_newlines=True,
                                        shell=True,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE
                                        )

    def __enter__(self):
        return self

    def __exit__(self, etype, value, traceback):
        """ Closes all memcr processess including worker processes waiting for restore command.
        Sends SIGINT (-2) signal. If SIGKILL would be used instead of SIGKILL, memcr would not
        be able to close its worker processes.
        """
        test_utils.print_log("Stopping memcr", test_utils.Severity.debug)
        subprocess.run(["sudo", "pkill", "-2", "memcr"])


def get_container_state(container_id):
    """ Returns state of container

    Parameters:
    container_id (string): name of container

    Returns:
    string: container state or None if getting container info failed
    """
    process = test_utils.dobby_tool_command("info", container_id)

    if not process.stdout.startswith("{"):
        return None

    info_json = json.loads(process.stdout)
    return info_json.get("state")


def get_container_pids(container_id):
    """ Returns list of pids running in container

    Parameters:
    container_id (string): name of container

    Returns:
    list of pids or [] if getting container info failed
    """
    process = test_utils.dobby_tool_command("info", container_id)

    if not process.stdout.startswith("{"):
        return []

    info_json = json.loads(process.stdout)
    return info_json.get("pids")


def get_checkpointed_pids(memcr_dump_dir = "/media/apps/memcr/"):
    """ Returns pids of processes that are currently checkpointed by memcr.
    Memcr stores memory of checkpointed processes in files named 'pages-<pid>.img'.
    By default files are stored in /media/apps/memcr/

    Parameters:
    memcr_dump_dir (string): default location of memcr pages files

    Returns:
    list of pids
    """
    prefix = "pages-"
    sufix = ".img"
    p = Path(memcr_dump_dir)

    checkpointed_pids = [int(x.name[len(prefix):-len(sufix)])
                            for x in p.iterdir()
                            if x.is_file() and x.name.startswith("pages-") and x.name.endswith(".img")]
    test_utils.print_log("checkpointed pids: [" + " ".join(map(str, checkpointed_pids)) + "]", test_utils.Severity.debug)
    return checkpointed_pids


def check_pids_checkpointed(pids):
    """ Checks if all pids from pids list are currently checkpointed

    Parameters:
    pids (list(int)): list of pids

    Returns:
    True if all pids from input parameter are checkpointed, False otherwise
    """
    checkpointed_pids = get_checkpointed_pids()
    for p in pids:
        if p not in checkpointed_pids:
            return False

    return True


def check_pids_restored(pids):
    """ Checks if all pids from pids list are currently restored (not checkpointed)

    Parameters:
    pids (list(int)): list of pids

    Returns:
    True if non of pids from input parameter is checkpointed, False otherwise
    """
    checkpointed_pids = get_checkpointed_pids()
    for p in pids:
        if p in checkpointed_pids:
            return False

    return True


def basic_memcr_test(container_id):
    with test_utils.dobby_daemon(), memcr(), test_utils.untar_bundle(container_id) as bundle_path:

        # start container
        command = ["DobbyTool", "start", container_id, bundle_path]
        test_utils.run_command_line(command)

        # give dobby some time to start container
        sleep(1)

        # check container is in running state
        if get_container_state(container_id) != "running":
            return False, "Unable to start container"

        # store container pids
        pids = get_container_pids(container_id)
        test_utils.print_log("container pids: [" + " ".join(map(str, pids)) + "]", test_utils.Severity.debug)

        # hibernate container
        test_utils.dobby_tool_command("hibernate", container_id)

        # give memcr some time to checkpoint everything
        sleep(1)

        # check container is hibernated
        if get_container_state(container_id) != "hibernated":
            return False, "Failed to hibernate container"

        # check if all processes were checkpointed
        if not check_pids_checkpointed(pids):
            return False, "Not all pids checkpointed"

        # wakeup/restore the container
        test_utils.dobby_tool_command("wakeup", container_id)

        # give memcr some time to wakeup container
        sleep(1)

        # check container is running again
        if get_container_state(container_id) != "running":
            return False, "Failed to wakeup container"

        # check if all processes were restored
        if not check_pids_restored(pids):
            return False, "Not all pids restored"

        return True, "Test passed"


def execute_test():
    output_table = []

    for test in tests:
        result = globals()[test.test_func](test.container_id)
        output = test_utils.create_simple_test_output(test, result[0], result[1])
        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
