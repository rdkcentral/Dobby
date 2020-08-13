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
from time import sleep
import subprocess
from os.path import basename

# in case we would like to change container name
container_name = "network1"

tests = (
    test_utils.Test("Test communication to host",
                    container_name,
                    "DOBBY_TEST",
                    "Tests port forwarding from container to host."),
)


class netcat_listener:
    """ Starts netcat listener on port 7357 """
    def __init__(self):

        # as this process is running infinitely we cannot use run_command_line as it waits for execution to end
        self.subproc = subprocess.Popen(["netcat",
                                         "-l",
                                         "localhost",
                                         "7357"
                                        ],
                                        universal_newlines=True,
                                        stdout=subprocess.PIPE,
                                        stderr=subprocess.PIPE)

    def __enter__(self):
        return self

    def __exit__(self, etype, value, traceback):
        self.subproc.kill()

    def get_output(self):
        # kill the process
        self.subproc.kill()

        # read stdout and return it
        output = self.subproc.communicate()
        return output[0]


def execute_test():

    # We use a preconfigured tarball bundle for this test, the container has the following arg:
    #
    #   echo DOBBY_TEST | netcat 100.64.11.1 7357
    #
    # This will send 'DOBBY_TEST' to the dobby0 bridge device at port 7357. We're expecting
    # this to be received on the listening port 7357 on the localhost because of the added
    # networking configuration in the bundle config:
    #
    #   "portForwarding": {
    #       "containerToHost": [
    #           {
    #               "port": 7357,
    #               "protocol": "udp"
    #           },
    #           {
    #               "port": 7357,
    #               "protocol": "tcp"
    #           }
    #       ]
    #   }
    #

    # netcat is not available on RDK builds, skip test
    if test_utils.selected_platform == test_utils.Platforms.xi_6:
        return test_utils.print_unsupported_platform(basename(__file__), test_utils.selected_platform)

    output_table = []

    with test_utils.dobby_daemon(), netcat_listener() as nc, test_utils.untar_bundle(container_name) as bundle_path:
        # Test 0
        test = tests[0]
        command = ["DobbyTool",
                   "start",
                   container_name,
                   bundle_path]

        status = test_utils.run_command_line(command)

        message = ""
        result = True

        # give container time to start and send message before checking netcat listener
        sleep(2)

        nc_message = nc.get_output().rstrip("\n")

        # check if netcat listener received message
        if test.expected_output.lower() not in nc_message.lower():
            message = "Received '%s' from container, expected '%s'" % (nc_message.lower(), test.expected_output.lower())
            result = False
        else:
            message = "Successfully received message '%s' from container" % nc_message

        output = test_utils.create_simple_test_output(test, result, message, status.stderr)
        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)

    execute_test()
