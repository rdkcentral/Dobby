# Automatic tests for dobby based features

## Requirements
This test runner uses only basic python modules. This means that if you have python3 installed you can run it without any 'pip install *' commands. But because in yocto some basic modules are removed below there is list of all modules used in testcases:
argparse
collections
enum
json
multiprocessing
os
re
signal
subprocess
time


## Running tests
To run single test use 'python3 test_name'. To run all test cases use 'python3 runner.py'.


```
optional arguments:
  -h, --help            show this help message and exit
  -v {0,1,2,3,4,5}, --verbosity {0,1,2,3,4,5}
                        Current test verbosity, possible options are: 0 =
                        no_log, 1 = error, 2 = warning, 3 = info, 4 = debug, 5
                        = log_all
  -p {0,1,2}, --platform {0,1,2}
                        Platform on which tests are performed, some tests can
                        be platform specific, possible options are: 0 =
                        no_selection, 1 = virtual_machine, 2 = xi_6
```

## Writing tests
To write new test you need:
1. Import test_utils inside new test
2. Have inside function called "execute_test" with no arguments
3. Single test cases should be either type 'test_utils.Test' or implement all fields of 'test_utils.Test' + some case specific ones
4. Output of single test cases should be time 'test_utils.TestResult', and should be stored in table called 'output_table'
	* To create new output usually using 'test_utils.create_simple_test_output' is enough
	* To print results for user use 'test_utils.print_single_result'
	* Function 'execute_test' should return 'test_utils.count_print_results(output_table)' or 'test_utils.print_unsupported_platform'
5. Add 'if __name__ == "__main__":' part same as in other tests
6. After writing new test add it to 'runner.py' to table 'supported_tests'

## Example test
```
import test_utils

tests = (
    test_utils.Test("Short Name 1",
                    "container_id_1",
                    "String to search for",
                    "long description of testcase 1"),
	test_utils.Test("Short Name 2",
                    "container_id_2",
                    "String to search for 2",
                    "long description of testcase 2"),
)


def execute_test():
    output_table = []

    for test in tests:
        # do some operations here

        output = test_utils.create_simple_test_output(test, result)
        output_table.append(output)
        test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
```
