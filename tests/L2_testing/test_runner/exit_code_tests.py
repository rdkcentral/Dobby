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

"""
L2 integration tests for RDKEMW-21185: Add exitcode on container stopped event.

These tests verify that DobbyDaemon emits the StoppedWithStatus D-Bus signal
when a container exits, and that the raw waitpid status carried by the signal
encodes the correct exit code.

Tests:
  1. exit_code_normal_exit  — container exits with code 42; verify signal
                               carries a waitpid status where WEXITSTATUS == 42
  2. exit_code_zero         — container exits with code 0; verify exit code == 0
  3. stopped_signal_also_emitted — both StoppedWithStatus and plain Stopped
                               signals are emitted when a container stops
"""

import subprocess
import test_utils
import os
import sys
import threading
import select
import re
from time import sleep, monotonic
from os.path import basename

# ---------------------------------------------------------------------------
# Containers used by these tests.
# exit_with_code.json: runs "/bin/sh -c 'exit 42'" — exits with code 42.
# echo.json:            runs "echo Hello World"    — exits with code 0.
# ---------------------------------------------------------------------------
CONTAINER_EXIT_42   = "exit_with_code"
CONTAINER_EXIT_ZERO = "echo"

# D-Bus interface / member names as emitted by DobbyDaemon
DBUS_INTERFACE         = "org.rdk.dobby.ctrl1"
SIGNAL_STOPPED_STATUS  = "StoppedWithStatus"
SIGNAL_STOPPED_PLAIN   = "Stopped"

# Timeout (seconds) to wait for a D-Bus signal after starting a container
SIGNAL_WAIT_TIMEOUT = 10

tests = (
    test_utils.Test("exit_code_normal_exit",
                    CONTAINER_EXIT_42,
                    "",
                    "Container exiting with code 42 should produce a "
                    "StoppedWithStatus signal where WEXITSTATUS == 42"),
    test_utils.Test("exit_code_zero",
                    CONTAINER_EXIT_ZERO,
                    "",
                    "Container exiting with code 0 should produce a "
                    "StoppedWithStatus signal where WEXITSTATUS == 0"),
    test_utils.Test("stopped_signal_also_emitted",
                    CONTAINER_EXIT_42,
                    "",
                    "Both StoppedWithStatus and plain Stopped signals must be "
                    "emitted when a container stops"),
)


# ---------------------------------------------------------------------------
# D-Bus signal monitoring helpers
# ---------------------------------------------------------------------------

class DbusSignalCapture:
    """Runs dbus-monitor in the background and collects matching signals.

    Usage::

        with DbusSignalCapture(DBUS_INTERFACE, SIGNAL_STOPPED_STATUS) as cap:
            # ... start container ...
            signals = cap.wait_for_signals(count=1, timeout=10)
    """

    def __init__(self, interface, member):
        self._interface = interface
        self._member    = member
        self._lines     = []
        self._lock      = threading.Lock()
        self._event     = threading.Event()
        self._proc      = None
        self._thread    = None

    def __enter__(self):
        match_rule = ("type='signal',interface='%s',member='%s'"
                      % (self._interface, self._member))
        self._proc = subprocess.Popen(
            ["dbus-monitor", "--system", match_rule],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            universal_newlines=True
        )
        self._thread = threading.Thread(target=self._reader, daemon=True)
        self._thread.start()
        return self

    def __exit__(self, *_):
        if self._proc:
            self._proc.terminate()
            try:
                self._proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait()
        if self._thread:
            self._thread.join(timeout=2)

    def _reader(self):
        for line in self._proc.stdout:
            with self._lock:
                self._lines.append(line.rstrip())
            self._event.set()

    def wait_for_signals(self, count=1, timeout=SIGNAL_WAIT_TIMEOUT):
        """Block until *count* signal blocks have been captured, or timeout."""
        deadline = monotonic() + timeout
        while monotonic() < deadline:
            with self._lock:
                # dbus-monitor emits one "signal" header line per signal
                headers = [l for l in self._lines
                           if "member=" + self._member in l]
                if len(headers) >= count:
                    return list(self._lines)
            remaining = deadline - monotonic()
            self._event.wait(timeout=min(0.2, remaining))
            self._event.clear()
        with self._lock:
            return list(self._lines)


def extract_int32_args(monitor_output, signal_member):
    """Parse int32 values that follow a matching signal header in dbus-monitor
    output.

    dbus-monitor prints signal data like::

        signal time=... interface=org.rdk.dobby.ctrl1 member=StoppedWithStatus
           int32 7
           string "mycontainer"
           int32 2621440

    Returns a list of (descriptor, name, raw_status) tuples for each match.
    """
    results = []
    lines   = monitor_output if isinstance(monitor_output, list) else \
              monitor_output.splitlines()

    i = 0
    while i < len(lines):
        if "member=" + signal_member in lines[i]:
            # Collect subsequent value lines
            ints    = []
            strings = []
            j = i + 1
            while j < len(lines) and not lines[j].strip().startswith("signal "):
                stripped = lines[j].strip()
                m_int = re.match(r'^int32\s+(-?\d+)$', stripped)
                m_str = re.match(r'^string\s+"(.*)"$', stripped)
                if m_int:
                    ints.append(int(m_int.group(1)))
                if m_str:
                    strings.append(m_str.group(1))
                j += 1

            # StoppedWithStatus carries (int32 cd, string id, int32 rawStatus)
            if len(ints) >= 2 and len(strings) >= 1:
                results.append({
                    "descriptor": ints[0],
                    "name":       strings[0],
                    "raw_status": ints[1]
                })
        i += 1

    return results


def wexitstatus(raw_status):
    """Return the exit code from a raw waitpid status (WEXITSTATUS macro)."""
    return (raw_status >> 8) & 0xFF


def wifexited(raw_status):
    """Return True if the process exited normally (WIFEXITED macro)."""
    return (raw_status & 0x7F) == 0


# ---------------------------------------------------------------------------
# Individual test functions
# ---------------------------------------------------------------------------

def test_exit_code_normal_exit():
    """Verify WEXITSTATUS == 42 in StoppedWithStatus for exit_with_code container."""
    test_utils.print_log("Starting test: exit_code_normal_exit",
                         test_utils.Severity.debug)

    spec_path = test_utils.get_container_spec_path(CONTAINER_EXIT_42)

    with DbusSignalCapture(DBUS_INTERFACE, SIGNAL_STOPPED_STATUS) as cap:
        proc = test_utils.run_command_line(
            ["DobbyTool", "start", CONTAINER_EXIT_42, spec_path])

        if "started '%s' container" % CONTAINER_EXIT_42 not in proc.stdout:
            return False, "Container did not start: %s" % proc.stdout

        monitor_lines = cap.wait_for_signals(count=1)

    entries = extract_int32_args(monitor_lines, SIGNAL_STOPPED_STATUS)
    matching = [e for e in entries if e["name"] == CONTAINER_EXIT_42]

    if not matching:
        return False, ("StoppedWithStatus signal not received for '%s'. "
                       "dbus-monitor output:\n%s"
                       % (CONTAINER_EXIT_42, "\n".join(monitor_lines)))

    raw_status = matching[0]["raw_status"]
    if not wifexited(raw_status):
        return False, ("Container '%s' did not exit normally "
                       "(raw_status=0x%04x)" % (CONTAINER_EXIT_42, raw_status))

    actual_exit_code = wexitstatus(raw_status)
    if actual_exit_code != 42:
        return False, ("Expected exit code 42, got %d "
                       "(raw_status=0x%04x)" % (actual_exit_code, raw_status))

    return True, "StoppedWithStatus signal received with correct exit code 42"


def test_exit_code_zero():
    """Verify WEXITSTATUS == 0 in StoppedWithStatus for a container that exits cleanly."""
    test_utils.print_log("Starting test: exit_code_zero",
                         test_utils.Severity.debug)

    spec_path = test_utils.get_container_spec_path(CONTAINER_EXIT_ZERO)

    with DbusSignalCapture(DBUS_INTERFACE, SIGNAL_STOPPED_STATUS) as cap:
        proc = test_utils.run_command_line(
            ["DobbyTool", "start", CONTAINER_EXIT_ZERO, spec_path])

        if "started '%s' container" % CONTAINER_EXIT_ZERO not in proc.stdout:
            return False, "Container did not start: %s" % proc.stdout

        monitor_lines = cap.wait_for_signals(count=1)

    entries = extract_int32_args(monitor_lines, SIGNAL_STOPPED_STATUS)
    matching = [e for e in entries if e["name"] == CONTAINER_EXIT_ZERO]

    if not matching:
        return False, ("StoppedWithStatus signal not received for '%s'. "
                       "dbus-monitor output:\n%s"
                       % (CONTAINER_EXIT_ZERO, "\n".join(monitor_lines)))

    raw_status = matching[0]["raw_status"]
    if not wifexited(raw_status):
        return False, ("Container '%s' did not exit normally "
                       "(raw_status=0x%04x)" % (CONTAINER_EXIT_ZERO, raw_status))

    actual_exit_code = wexitstatus(raw_status)
    if actual_exit_code != 0:
        return False, ("Expected exit code 0, got %d "
                       "(raw_status=0x%04x)" % (actual_exit_code, raw_status))

    return True, "StoppedWithStatus signal received with correct exit code 0"


def test_stopped_signal_also_emitted():
    """Verify that plain Stopped AND StoppedWithStatus are both emitted."""
    test_utils.print_log("Starting test: stopped_signal_also_emitted",
                         test_utils.Severity.debug)

    spec_path = test_utils.get_container_spec_path(CONTAINER_EXIT_42)

    # Monitor both signals concurrently
    with DbusSignalCapture(DBUS_INTERFACE, SIGNAL_STOPPED_STATUS) as status_cap, \
         DbusSignalCapture(DBUS_INTERFACE, SIGNAL_STOPPED_PLAIN) as plain_cap:

        proc = test_utils.run_command_line(
            ["DobbyTool", "start", CONTAINER_EXIT_42, spec_path])

        if "started '%s' container" % CONTAINER_EXIT_42 not in proc.stdout:
            return False, "Container did not start: %s" % proc.stdout

        status_lines = status_cap.wait_for_signals(count=1)
        plain_lines  = plain_cap.wait_for_signals(count=1)

    got_status = any("member=" + SIGNAL_STOPPED_STATUS in l
                     for l in status_lines)
    got_plain  = any("member=" + SIGNAL_STOPPED_PLAIN  in l
                     for l in plain_lines)

    if not got_status:
        return False, "StoppedWithStatus signal was not emitted"
    if not got_plain:
        return False, "Plain Stopped signal was not emitted"

    return True, ("Both '%s' and '%s' signals were emitted"
                  % (SIGNAL_STOPPED_STATUS, SIGNAL_STOPPED_PLAIN))


# ---------------------------------------------------------------------------
# Test runner
# ---------------------------------------------------------------------------

def execute_test():
    output_table = []

    test_functions = [
        (tests[0], test_exit_code_normal_exit),
        (tests[1], test_exit_code_zero),
        (tests[2], test_stopped_signal_also_emitted),
    ]

    with test_utils.dobby_daemon():
        for test, fn in test_functions:
            result, message = fn()
            output = test_utils.create_simple_test_output(test, result, message)
            output_table.append(output)
            test_utils.print_single_result(output)

    return test_utils.count_print_results(output_table)


if __name__ == "__main__":
    test_utils.parse_arguments(__file__)
    execute_test()
