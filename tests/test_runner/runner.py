import test_utils
import basic_sanity_tests
import container_manipulations
import command_line_containers
import start_from_bundle
import bundle_generation
import plugin_launcher
import thunder_plugin
import gui_containers

from time import sleep

supported_tests = [basic_sanity_tests,
                   container_manipulations,
                   command_line_containers,
                   start_from_bundle,
                   bundle_generation,
                   plugin_launcher,
                   thunder_plugin,
                   gui_containers]


def run_all_tests():
    success_count = 0
    total_count = 0
    tested_groups_count = 0
    for test in supported_tests:
        test_utils.print_log("\nExecuting test \"%s\"" % test.__name__, test_utils.Severity.info)
        success, total = test.execute_test()
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


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    run_all_tests()
