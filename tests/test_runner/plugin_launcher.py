import test_utils

container_name = "sleepy-thunder"

tests = (
    test_utils.Test("Run plugin launcher",
                    container_name,
                    "Plugins run successfully",
                    "Runs plugins launcher and check if it runned properly"),
)


def execute_test():

    output_table = []

    with test_utils.dobby_daemon(), test_utils.untar_bundle(container_name) as bundle_path:

        # Test 0
        test = tests[0]
        command = ["DobbyPluginLauncher",
                   "-v",
                   "-h", "createRuntime",
                   "-c", bundle_path + "/config.json"]

        status = test_utils.run_command_line(command)

        result = test.expected_output.lower() in status.stderr.lower()
        output = test_utils.create_simple_test_output(test, result, log_content=status.stderr)

        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
