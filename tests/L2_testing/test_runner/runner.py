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
import basic_sanity_tests
import container_manipulations
import command_line_containers
import start_from_bundle
import bundle_generation
import plugin_launcher
import thunder_plugin
import gui_containers
import network_tests
import pid_limit_tests
import sys

from time import sleep

supported_tests = [basic_sanity_tests,
                   container_manipulations,
                   bundle_generation,
                   plugin_launcher,
#                   command_line_containers, # Commented these tests because they are all failing in the workflow and will be enabled once they are resolved
#                   start_from_bundle,
#                   thunder_plugin,
#                   network_tests,
                   gui_containers,
                   pid_limit_tests]

def run_all_tests():
    success_count = 0
    total_count = 0
    tested_groups_count = 0
    for test in supported_tests:
        test_utils.print_log("\nExecuting test Test Group: \"%s\"" % test.__name__, test_utils.Severity.info)
        if (test_utils.selected_platform == test_utils.Platforms.virtual_machine) and (test.__name__ == "gui_containers"):
            test_utils.print_unsupported_platform("gui_containers.py", test_utils.selected_platform)
        else:
            success, total = test.execute_test()
            test_utils.print_log("\nTest Group: \"%s\"" % test.__name__, test_utils.Severity.info)
            test_utils.print_log("Total Tests: %d " % total, test_utils.Severity.info)
            test_utils.print_log("Total Passed Tests: %d " % success, test_utils.Severity.info)
            success_count += success
            total_count += total
            if total > 0:
                tested_groups_count += 1
            sleep(1)

    test_utils.print_log("\n\nSummary:", test_utils.Severity.info)
    skipped_test_count = len(supported_tests) - tested_groups_count
    if skipped_test_count:
        test_utils.print_log("Skipped %d test groups" % skipped_test_count, test_utils.Severity.info)
    test_utils.print_log("Tested %d test groups" % tested_groups_count, test_utils.Severity.info)
    test_utils.print_results(success_count, total_count)
    if success_count != total_count:
        sys.exit(1)

if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    run_all_tests()
