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
import memcr_tests
import annotation_tests
import sys
import json

from time import sleep

supported_tests = [basic_sanity_tests,
                   container_manipulations,
                   bundle_generation,
                   plugin_launcher,
                   thunder_plugin,
                   ]

def run_all_tests():
    success_count = 0
    total_count = 0
    tested_groups_count = 0
    testsuites_info = []
    all_test_report = {}

    for test in supported_tests:
        test_utils.print_log("\nExecuting test Test Group: \"%s\"" % test.__name__, test_utils.Severity.info)
        if (test_utils.selected_platform  == test_utils.Platforms.vagrant_vm ) and (test.__name__ == "gui_containers"):
            test_utils.print_unsupported_platform(test.__name__, test_utils.selected_platform)
        # Skipped these tests because they are all failing in the workflow and will be enabled once they are resolved
        elif (test_utils.selected_platform == test_utils.Platforms.github_workflow_vm) and (test.__name__ == "gui_containers"):
            test_utils.print_unsupported_platform(test.__name__, test_utils.selected_platform)
        else:
            success, total = test.execute_test()
            test_utils.print_log("\nTest Group: \"%s\"" % test.__name__, test_utils.Severity.info)
            test_utils.print_log("Total Tests: %d " % total, test_utils.Severity.info)
            test_utils.print_log("Total Passed Tests: %d " % success, test_utils.Severity.info)
            success_count += success
            total_count += total
            testsuites_info.append({"name":test.__name__,"tests":total,"Passed Tests":success,"Failed Tests":total - success})
            with open('test_results.json', 'r') as json_file:
                current_test_result = json.load(json_file)
            testsuites_info[tested_groups_count]['testsuite'] = []
            testsuites_info[tested_groups_count]["testsuite"].append(current_test_result)
            if total > 0:
                tested_groups_count += 1
            sleep(1)

    test_utils.print_log("\n\nSummary:", test_utils.Severity.info)
    skipped_test_count = len(supported_tests) - tested_groups_count
    json_file_path = 'DobbyL2TestResults.json'
    all_test_report["Total tests"] = total_count
    all_test_report["Passed tests"] = success_count
    all_test_report["Failed Tests"] = total_count - success_count
    all_test_report["Skipped testsuites"] = skipped_test_count
    all_test_report["testsuites"] = []
    all_test_report["testsuites"].append(testsuites_info)
    with open(json_file_path, 'w') as json_file:
        json.dump(all_test_report, json_file, indent=2)
    if skipped_test_count:
        test_utils.print_log("Skipped %d test groups" % skipped_test_count, test_utils.Severity.info)
    test_utils.print_log("Tested %d test groups" % tested_groups_count, test_utils.Severity.info)
    test_utils.print_results(success_count, total_count)
    if success_count != total_count:
        sys.exit(1)

if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    run_all_tests()
