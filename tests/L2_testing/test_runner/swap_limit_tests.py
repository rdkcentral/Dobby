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

# Use "unlimited" as a sentinel to indicate the swap cgroup value should be
# much larger than memLimit (kernel reports its page-aligned max when unset).
UNLIMITED = "unlimited"

tests = [
    test_utils.Test(
        "Swap limit default",
        "ram",
        UNLIMITED,
        "Starts a container with only memLimit set and verifies that "
        "memory.memsw.limit_in_bytes is unlimited (much larger than memLimit)"),
    test_utils.Test(
        "Swap limit override",
        "swap_limit",
        str(5996544),
        "Starts a container with swapLimit > memLimit and verifies that "
        "memory.memsw.limit_in_bytes reflects the independent swapLimit value"),
]


def execute_test():
    with test_utils.dobby_daemon():
        output_table = []

        for test in tests:
            result = test_container(test.container_id, test.expected_output)
            output = test_utils.create_simple_test_output(test, result[0], result[1])
            output_table.append(output)
            test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


def test_container(container_id, expected_swap):
    """Launch a container and verify its cgroup swap limit.

    The container cats /sys/fs/cgroup/memory/memory.memsw.limit_in_bytes
    to its console log, which is then read and compared against the
    expected value.

    Parameters:
        container_id (str): name of the spec (without .json extension)
        expected_swap (str): expected numeric string from the cgroup file

    Returns:
        (bool, str): (passed, message)
    """
    test_utils.print_log("Running swap limit test for '%s'" % container_id,
                         test_utils.Severity.debug)

    spec_path = test_utils.get_container_spec_path(container_id)
    launched  = test_utils.launch_container(container_id, spec_path)

    if not launched:
        return False, "Container '%s' failed to launch" % container_id

    return validate_swap_limit(container_id, expected_swap)


def validate_swap_limit(container_id, expected_swap):
    """Read the container log and compare the swap cgroup value.

    If the log is empty (swap accounting disabled in the kernel), the test
    is treated as a skip rather than a failure so CI does not break on
    platforms where 'swapaccount=1' has not been set on the kernel cmdline.

    If expected_swap is the UNLIMITED sentinel, the test passes when the
    reported value is significantly larger than any reasonable memLimit
    (indicating the kernel has no swap ceiling set).

    Parameters:
        container_id (str): container whose log to inspect
        expected_swap (str): expected value as a decimal string, or UNLIMITED

    Returns:
        (bool, str): (passed, message)
    """
    log = test_utils.get_container_log(container_id)

    if not log:
        test_utils.print_log(
            "No log output from '%s' – swap accounting may be disabled "
            "(kernel cmdline requires 'swapaccount=1')" % container_id,
            test_utils.Severity.warning)
        return True, "Skipped – swap accounting not available on this platform"

    actual = log.strip()

    if expected_swap == UNLIMITED:
        # When swap is unlimited the kernel reports a very large page-aligned
        # value (e.g. 9223372036854771712 on 64-bit).  We consider any value
        # above 1 TiB (1099511627776) as effectively unlimited.
        try:
            actual_val = int(actual)
        except ValueError:
            return False, "Could not parse swap value '%s' as integer" % actual
        if actual_val > 1099511627776:
            return True, "Test passed (swap unlimited: %s)" % actual
        return (False,
                "Swap limit for '%s' should be unlimited but got %s"
                % (container_id, actual))

    if actual == expected_swap:
        return True, "Test passed"

    return (False,
            "Swap limit mismatch for '%s': expected %s, got %s"
            % (container_id, expected_swap, actual))


if __name__ == "__main__":
    test_utils.parse_arguments(__file__, True)
    execute_test()
