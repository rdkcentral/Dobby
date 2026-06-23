#!/usr/bin/env python3
# If not stated otherwise in this file or this component's LICENSE file the
# following copyright and licenses apply:
#
# Copyright 2026 RDK Management
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
#
"""
Tests for compare_coverage.py

Covers:
  - Unit tests for parse_lcov_coverage(), load_baseline(), _suite_analysis()
  - Integration tests (subprocess) simulating all gate scenarios listed in the
    Coverage Gate implementation spec.

Workflow-level scenarios (L0 job fails / L1 job fails / both fail) are handled
by GitHub Actions' implicit success() dependency check on the coverage-gate job
and cannot be tested at the Python script level; they are documented inline.
"""

import json
import os
import subprocess
import sys
import tempfile
import unittest

# ---------------------------------------------------------------------------
# Import the module under test
# ---------------------------------------------------------------------------
SCRIPTS_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPTS_DIR)

import compare_coverage  # noqa: E402  (after sys.path manipulation)

THRESHOLD = compare_coverage.THRESHOLD  # 75.0


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------

def _make_lcov(lines_found: int, lines_hit: int) -> str:
    """Minimal valid lcov .info content with the given LF/LH counts."""
    return (
        "SF:src/fake.cpp\n"
        f"LF:{lines_found}\n"
        f"LH:{lines_hit}\n"
        "end_of_record\n"
    )


def _write_lcov(tmp_dir: str, name: str, lines_found: int, lines_hit: int) -> str:
    """Write an lcov file and return its absolute path."""
    path = os.path.join(tmp_dir, name)
    with open(path, "w") as fh:
        fh.write(_make_lcov(lines_found, lines_hit))
    return path


def _write_baseline(tmp_dir: str, data: dict, name: str = "baseline.json") -> str:
    """Serialise *data* to JSON and return the path."""
    path = os.path.join(tmp_dir, name)
    with open(path, "w") as fh:
        json.dump(data, fh)
    return path


def _run_script(*args: str) -> subprocess.CompletedProcess:
    """Invoke compare_coverage.py as a subprocess and return the result."""
    cmd = [sys.executable, os.path.join(SCRIPTS_DIR, "compare_coverage.py"), *args]
    return subprocess.run(cmd, capture_output=True, text=True)


# ===========================================================================
# Unit tests — parse_lcov_coverage()
# ===========================================================================

class TestParseLcovCoverage(unittest.TestCase):
    """Tests for the lcov .info parser."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _write(self, name: str, content: str) -> str:
        path = os.path.join(self.tmp, name)
        with open(path, "w") as fh:
            fh.write(content)
        return path

    # --- Missing / empty inputs -------------------------------------------------

    def test_none_path_returns_none(self):
        self.assertIsNone(compare_coverage.parse_lcov_coverage(None))

    def test_empty_path_returns_none(self):
        self.assertIsNone(compare_coverage.parse_lcov_coverage(""))

    def test_nonexistent_file_returns_none(self):
        self.assertIsNone(compare_coverage.parse_lcov_coverage("/no/such/file.info"))

    def test_empty_file_returns_none(self):
        p = self._write("empty.info", "")
        self.assertIsNone(compare_coverage.parse_lcov_coverage(p))

    def test_no_lf_data_returns_none(self):
        p = self._write("no_lf.info", "SF:foo.cpp\nend_of_record\n")
        self.assertIsNone(compare_coverage.parse_lcov_coverage(p))

    def test_lf_zero_returns_none(self):
        p = self._write("zero_lf.info", "SF:foo.cpp\nLF:0\nLH:0\nend_of_record\n")
        self.assertIsNone(compare_coverage.parse_lcov_coverage(p))

    # --- Basic coverage values --------------------------------------------------

    def test_100_percent(self):
        p = _write_lcov(self.tmp, "full.info", 100, 100)
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 100.0)

    def test_75_percent_exact(self):
        p = _write_lcov(self.tmp, "seventy_five.info", 100, 75)
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 75.0)

    def test_zero_percent(self):
        p = _write_lcov(self.tmp, "zero_pct.info", 100, 0)
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 0.0)

    def test_partial_coverage(self):
        # 150 / 200 = 75.0 %
        p = _write_lcov(self.tmp, "partial.info", 200, 150)
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 75.0)

    # --- Multi-record aggregation -----------------------------------------------

    def test_aggregates_multiple_records(self):
        # 80 + 60 = 140 hit out of 200 → 70.0 %
        content = (
            "SF:a.cpp\nLF:100\nLH:80\nend_of_record\n"
            "SF:b.cpp\nLF:100\nLH:60\nend_of_record\n"
        )
        p = self._write("multi.info", content)
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 70.0)

    # --- Malformed data ---------------------------------------------------------

    def test_malformed_lf_ignored_gracefully(self):
        # LF with a non-numeric value; total_found stays 0 → None
        p = self._write("bad_lf.info", "SF:a.cpp\nLF:abc\nLH:50\nend_of_record\n")
        self.assertIsNone(compare_coverage.parse_lcov_coverage(p))

    def test_malformed_lh_ignored_gracefully(self):
        # LH with garbage value; LF is valid, so LF=100, LH=0 → 0.0 %
        p = self._write("bad_lh.info", "SF:a.cpp\nLF:100\nLH:xyz\nend_of_record\n")
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 0.0)

    def test_entirely_non_lcov_content(self):
        p = self._write("corrupt.info", "THIS IS NOT A VALID LCOV FILE\n")
        self.assertIsNone(compare_coverage.parse_lcov_coverage(p))

    # --- Rounding ---------------------------------------------------------------

    def test_rounds_to_two_decimal_places(self):
        # 1/3 ≈ 33.33 %
        p = _write_lcov(self.tmp, "third.info", 3, 1)
        self.assertEqual(compare_coverage.parse_lcov_coverage(p), 33.33)


# ===========================================================================
# Unit tests — load_baseline()
# ===========================================================================

class TestLoadBaseline(unittest.TestCase):
    """Tests for the JSON baseline loader."""

    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _write_json(self, name: str, content: str) -> str:
        path = os.path.join(self.tmp, name)
        with open(path, "w") as fh:
            fh.write(content)
        return path

    # --- Missing / empty inputs -------------------------------------------------

    def test_none_returns_empty_dict(self):
        self.assertEqual(compare_coverage.load_baseline(None), {})

    def test_empty_path_returns_empty_dict(self):
        self.assertEqual(compare_coverage.load_baseline(""), {})

    def test_nonexistent_file_returns_empty_dict(self):
        self.assertEqual(compare_coverage.load_baseline("/no/such/file.json"), {})

    # --- Valid JSON -------------------------------------------------------------

    def test_valid_baseline_with_l0_and_l1(self):
        p = _write_baseline(self.tmp, {"L0": 80.0, "L1": 85.0})
        self.assertEqual(compare_coverage.load_baseline(p), {"L0": 80.0, "L1": 85.0})

    def test_valid_empty_json_object(self):
        p = _write_baseline(self.tmp, {})
        self.assertEqual(compare_coverage.load_baseline(p), {})

    def test_valid_baseline_extra_keys_preserved(self):
        data = {"L0": 80.0, "L1": 85.0, "commit": "abc123", "timestamp": "2026-01-01"}
        p = _write_baseline(self.tmp, data)
        self.assertEqual(compare_coverage.load_baseline(p), data)

    # --- Invalid JSON -----------------------------------------------------------

    def test_malformed_json_returns_empty_dict(self):
        p = self._write_json("bad.json", "{not valid json}")
        result = compare_coverage.load_baseline(p)
        self.assertEqual(result, {})

    def test_truncated_json_returns_empty_dict(self):
        p = self._write_json("truncated.json", '{"L0": 80')
        self.assertEqual(compare_coverage.load_baseline(p), {})

    def test_empty_file_returns_empty_dict(self):
        p = self._write_json("empty.json", "")
        self.assertEqual(compare_coverage.load_baseline(p), {})

    # --- Non-dict JSON ----------------------------------------------------------

    def test_json_array_returns_empty_dict(self):
        import io, contextlib
        p = self._write_json("list.json", "[1, 2, 3]")
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            result = compare_coverage.load_baseline(p)
        self.assertEqual(result, {})
        self.assertIn("WARNING", buf.getvalue())

    def test_json_string_returns_empty_dict(self):
        import io, contextlib
        p = self._write_json("str.json", '"just a string"')
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            result = compare_coverage.load_baseline(p)
        self.assertEqual(result, {})
        self.assertIn("WARNING", buf.getvalue())

    def test_json_number_returns_empty_dict(self):
        import io, contextlib
        p = self._write_json("num.json", "42")
        buf = io.StringIO()
        with contextlib.redirect_stderr(buf):
            result = compare_coverage.load_baseline(p)
        self.assertEqual(result, {})
        self.assertIn("WARNING", buf.getvalue())

    def test_json_null_returns_empty_dict(self):
        p = self._write_json("null.json", "null")
        # null is parsed as None, which is not a dict — Warning emitted, empty dict returned
        self.assertEqual(compare_coverage.load_baseline(p), {})


# ===========================================================================
# Unit tests — _suite_analysis()
# ===========================================================================

class TestSuiteAnalysis(unittest.TestCase):
    """
    Tests for the core gate analysis function.

    Gate passes (ok=True) when BOTH:
      1. current >= THRESHOLD (75.0)
      2. current >= baseline (regression check)

    SKIP when current is None.
    Regression check disabled when baseline is None or 0.0.
    """

    # --- Scenario 1: exceeds both threshold AND baseline → PASS ----------------

    def test_s1_exceeds_threshold_and_baseline(self):
        ok, _, _, reason = compare_coverage._suite_analysis(80.0, 77.0)
        self.assertTrue(ok)
        self.assertIsNone(reason)

    # --- Scenario 2: meets threshold exactly (75%) AND beats baseline → PASS ---

    def test_s2_meets_threshold_exactly_beats_baseline(self):
        ok, _, _, reason = compare_coverage._suite_analysis(75.0, 70.0)
        self.assertTrue(ok)
        self.assertIsNone(reason)

    # --- Scenario 3: meets baseline exactly, exceeds threshold → PASS ----------

    def test_s3_meets_baseline_exactly_exceeds_threshold(self):
        ok, _, _, reason = compare_coverage._suite_analysis(80.0, 80.0)
        self.assertTrue(ok)
        self.assertIsNone(reason)

    # --- Scenario 4: meets BOTH exactly (75.0 == threshold == baseline) → PASS -

    def test_s4_meets_both_exactly_at_threshold(self):
        ok, _, _, reason = compare_coverage._suite_analysis(75.0, 75.0)
        self.assertTrue(ok)
        self.assertIsNone(reason)

    # --- Scenario 5: exceeds threshold but BELOW baseline → FAIL (regression) --

    def test_s5_above_threshold_below_baseline(self):
        ok, result, _, reason = compare_coverage._suite_analysis(76.0, 80.0)
        self.assertFalse(ok)
        self.assertIsNotNone(reason)
        self.assertIn("dropped from baseline", reason)

    # --- Scenario 6: below threshold but meets/exceeds baseline → FAIL ----------

    def test_s6_below_threshold_meets_baseline(self):
        ok, _, _, reason = compare_coverage._suite_analysis(74.0, 70.0)
        self.assertFalse(ok)
        self.assertIsNotNone(reason)
        self.assertIn("below threshold", reason)
        self.assertNotIn("dropped from baseline", reason)  # regression check passed

    def test_s6b_below_threshold_equals_baseline(self):
        ok, _, _, reason = compare_coverage._suite_analysis(74.0, 74.0)
        self.assertFalse(ok)
        self.assertIsNotNone(reason)
        self.assertIn("below threshold", reason)
        self.assertNotIn("dropped from baseline", reason)  # regression check passed

    # --- Scenario 7: fails BOTH conditions → FAIL --------------------------------

    def test_s7_fails_both_conditions(self):
        ok, _, _, reason = compare_coverage._suite_analysis(70.0, 80.0)
        self.assertFalse(ok)
        self.assertIsNotNone(reason)
        self.assertIn("below threshold", reason)
        self.assertIn("dropped from baseline", reason)

    # --- Scenario 8: no baseline → threshold-only check -------------------------

    def test_no_baseline_above_threshold_passes(self):
        ok, _, delta, reason = compare_coverage._suite_analysis(80.0, None)
        self.assertTrue(ok)
        self.assertIsNone(reason)
        self.assertEqual(delta, "N/A")

    def test_no_baseline_below_threshold_fails(self):
        ok, _, _, reason = compare_coverage._suite_analysis(70.0, None)
        self.assertFalse(ok)
        self.assertIn("below threshold", reason)

    def test_no_baseline_at_threshold_exactly_passes(self):
        ok, _, _, reason = compare_coverage._suite_analysis(75.0, None)
        self.assertTrue(ok)
        self.assertIsNone(reason)

    # --- SKIP case: no current coverage -----------------------------------------

    def test_skip_when_current_is_none(self):
        ok, result, delta, reason = compare_coverage._suite_analysis(None, 80.0)
        self.assertFalse(ok, "Missing coverage data should be treated as WARN")
        self.assertIn("coverage data missing", result)
        self.assertEqual(delta, "N/A")
        self.assertIsNotNone(reason)
        self.assertIn("coverage data missing", reason)

    def test_skip_when_both_none(self):
        ok, result, delta, reason = compare_coverage._suite_analysis(None, None)
        self.assertFalse(ok)
        self.assertIn("coverage data missing", result)

    # --- Zero baseline: regression check disabled --------------------------------

    def test_zero_baseline_above_threshold_passes(self):
        ok, _, delta, reason = compare_coverage._suite_analysis(80.0, 0.0)
        self.assertTrue(ok)
        self.assertIsNone(reason)
        self.assertEqual(delta, "N/A", "Delta must be N/A for zero baseline")

    def test_zero_baseline_below_threshold_fails(self):
        ok, _, _, reason = compare_coverage._suite_analysis(70.0, 0.0)
        self.assertFalse(ok)
        self.assertIsNotNone(reason)
        self.assertIn("below threshold", reason)

    # --- Delta string correctness ------------------------------------------------

    def test_delta_positive(self):
        _, _, delta, _ = compare_coverage._suite_analysis(80.0, 77.0)
        self.assertEqual(delta, "+3.00%")

    def test_delta_negative(self):
        _, _, delta, _ = compare_coverage._suite_analysis(76.0, 80.0)
        self.assertEqual(delta, "-4.00%")

    def test_delta_zero(self):
        _, _, delta, _ = compare_coverage._suite_analysis(80.0, 80.0)
        self.assertEqual(delta, "+0.00%")

    def test_delta_na_when_no_baseline(self):
        _, _, delta, _ = compare_coverage._suite_analysis(80.0, None)
        self.assertEqual(delta, "N/A")

    # --- Boundary: one tick below threshold (74.99 is impossible from lcov,
    #     but 74.0 covers the just-below case) ----------------------------------

    def test_just_below_threshold_fails(self):
        # 74 / 100 = 74.0 %
        ok, _, _, reason = compare_coverage._suite_analysis(74.0, 70.0)
        self.assertFalse(ok)

    def test_just_at_threshold_passes(self):
        ok, _, _, reason = compare_coverage._suite_analysis(75.0, 70.0)
        self.assertTrue(ok)


# ===========================================================================
# Integration tests — main() via subprocess
# ===========================================================================

class TestMainIntegration(unittest.TestCase):
    """
    End-to-end simulation of every gate scenario.

    Each test invokes the script as a subprocess (exactly as GitHub Actions
    would) and asserts on exit code and stdout/stderr content.
    """

    def setUp(self):
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp, ignore_errors=True)

    # --- Helpers ----------------------------------------------------------------

    def _lcov(self, name: str, lf: int, lh: int) -> str:
        return _write_lcov(self.tmp, name, lf, lh)

    def _baseline(self, data: dict, name: str = "baseline.json") -> str:
        return _write_baseline(self.tmp, data, name)

    def _run(self, *args: str) -> subprocess.CompletedProcess:
        return _run_script(*args)

    # ===========================================================================
    # SCENARIO 1 — Coverage exceeds both threshold AND baseline
    # Expected: Gate PASSES (exit 0), baseline updates
    # ===========================================================================

    def test_s1_exceeds_threshold_and_baseline(self):
        bl = self._baseline({"L0": 77.0, "L1": 78.0})
        l0 = self._lcov("l0.info", 100, 80)  # 80 %
        l1 = self._lcov("l1.info", 100, 82)  # 82 %
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[PASS]", r.stdout)

    # ===========================================================================
    # SCENARIO 2 — Coverage meets threshold exactly (75%) and meets baseline
    # Expected: Gate PASSES (exit 0)
    # ===========================================================================

    def test_s2_meets_threshold_exactly_meets_baseline(self):
        bl = self._baseline({"L0": 70.0, "L1": 70.0})
        l0 = self._lcov("l0.info", 100, 75)  # 75.0 %
        l1 = self._lcov("l1.info", 100, 75)  # 75.0 %
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[PASS]", r.stdout)

    # ===========================================================================
    # SCENARIO 3 — Meets baseline exactly but exceeds threshold
    # Expected: Gate PASSES (exit 0)
    # ===========================================================================

    def test_s3_meets_baseline_exactly_exceeds_threshold(self):
        bl = self._baseline({"L0": 80.0, "L1": 80.0})
        l0 = self._lcov("l0.info", 100, 80)  # 80 % == baseline
        l1 = self._lcov("l1.info", 100, 80)  # 80 % == baseline
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    # ===========================================================================
    # SCENARIO 4 — Meets BOTH exactly (current == threshold == baseline == 75 %)
    # Expected: Gate PASSES (exit 0)
    # ===========================================================================

    def test_s4_meets_both_exactly(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 75)
        l1 = self._lcov("l1.info", 100, 75)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    # ===========================================================================
    # SCENARIO 5 — Exceeds threshold but falls BELOW baseline (regression)
    # Expected: Gate WARNS (exit 0 — informational only), [WARN] shown
    # ===========================================================================

    def test_s5_above_threshold_below_baseline(self):
        bl = self._baseline({"L0": 85.0, "L1": 85.0})
        l0 = self._lcov("l0.info", 100, 80)  # 80 % < 85 % baseline
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)
        self.assertIn("dropped from baseline", r.stdout)

    # ===========================================================================
    # SCENARIO 6 — Falls BELOW threshold but meets/exceeds baseline
    # Expected: Gate WARNS (exit 0 — informational only), [WARN] shown
    # ===========================================================================

    def test_s6_below_threshold_meets_baseline(self):
        bl = self._baseline({"L0": 70.0, "L1": 70.0})
        l0 = self._lcov("l0.info", 100, 74)  # 74 % < 75 % threshold
        l1 = self._lcov("l1.info", 100, 74)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)
        self.assertIn("below threshold", r.stdout)

    # ===========================================================================
    # SCENARIO 7 — Fails BOTH conditions (below threshold AND below baseline)
    # Expected: Gate WARNS (exit 0 — informational only), [WARN] shown
    # ===========================================================================

    def test_s7_fails_both_threshold_and_baseline(self):
        bl = self._baseline({"L0": 85.0, "L1": 85.0})
        l0 = self._lcov("l0.info", 100, 70)  # 70 % < threshold AND < baseline
        l1 = self._lcov("l1.info", 100, 70)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("below threshold", r.stdout)
        self.assertIn("dropped from baseline", r.stdout)

    # ===========================================================================
    # SCENARIO 8a — Baseline file is MISSING
    # Expected: Graceful fallback; threshold-only check; no crash
    # ===========================================================================

    def test_s8_baseline_missing_coverage_above_threshold(self):
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run(
            "--baseline", "/nonexistent/coverage-baseline.json",
            "--l0", l0, "--l1", l1,
        )
        # No baseline → regression skipped → threshold pass → exit 0
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_s8_baseline_missing_coverage_below_threshold(self):
        l0 = self._lcov("l0.info", 100, 70)  # 70 % < 75 %
        l1 = self._lcov("l1.info", 100, 70)
        r = self._run(
            "--baseline", "/nonexistent/coverage-baseline.json",
            "--l0", l0, "--l1", l1,
        )
        # Informational only — exit 0 even below threshold; [WARN] shown
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)

    # ===========================================================================
    # SCENARIO 9 — Baseline file contains invalid / malformed JSON
    # Expected: Warning emitted, treated as empty baseline, gate continues
    # ===========================================================================

    def test_s9_malformed_json_above_threshold(self):
        path = os.path.join(self.tmp, "malformed.json")
        with open(path, "w") as fh:
            fh.write("{this is not json}")
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", path, "--l0", l0, "--l1", l1)
        # Warning must appear in stderr
        self.assertIn("WARNING", r.stderr)
        # Fallback to empty baseline → threshold-only → pass
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_s9_malformed_json_below_threshold(self):
        path = os.path.join(self.tmp, "malformed2.json")
        with open(path, "w") as fh:
            fh.write("{bad json")
        l0 = self._lcov("l0.info", 100, 70)
        l1 = self._lcov("l1.info", 100, 70)
        r = self._run("--baseline", path, "--l0", l0, "--l1", l1)
        self.assertIn("WARNING", r.stderr)
        # Informational only — exit 0 even below threshold; [WARN] shown
        self.assertEqual(r.returncode, 0)
        self.assertIn("[WARN]", r.stdout)

    def test_s9_empty_json_file(self):
        path = os.path.join(self.tmp, "empty.json")
        with open(path, "w") as fh:
            fh.write("")
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", path, "--l0", l0, "--l1", l1)
        # Empty file → empty dict baseline → threshold-only → pass
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_s9_non_dict_json(self):
        path = os.path.join(self.tmp, "list_json.json")
        with open(path, "w") as fh:
            json.dump([1, 2, 3], fh)
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", path, "--l0", l0, "--l1", l1)
        # Non-dict JSON → WARNING emitted, empty dict → threshold-only → pass
        self.assertIn("WARNING", r.stderr)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    # ===========================================================================
    # SCENARIO 10 — L0 job fails
    # Coverage Gate does NOT trigger (workflow-level behaviour).
    #
    # GitHub Actions: coverage-gate has `needs: [trigger-L0, trigger-L1]`
    # with no custom `if:`.  The implicit success() check means coverage-gate
    # is SKIPPED whenever trigger-L0 fails.  update-baseline then sees
    # needs.coverage-gate.result == 'skipped' (not 'success') and is also
    # skipped.  This cannot be unit-tested here; it is enforced by the
    # workflow graph.
    # ===========================================================================

    def test_s10_l0_artifacts_absent_l1_passes(self):
        """
        Simulates the artifact-level effect: L0 .info absent (download step
        with continue-on-error:true produced no file), L1 coverage present
        and passing.  Script-level: L0 is WARN (data missing), L1 is PASS.
        """
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l1 = self._lcov("l1.info", 100, 80)  # L0 omitted intentionally
        r = self._run("--baseline", bl, "--l1", l1)
        # L0 WARN (missing) → overall WARN, but exit 0 (informational)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("coverage data missing", r.stdout)
        self.assertIn("[WARN]", r.stdout)

    # ===========================================================================
    # SCENARIO 11 — L1 job fails (symmetric to scenario 10)
    # ===========================================================================

    def test_s11_l1_artifacts_absent_l0_passes(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)  # L1 omitted intentionally
        r = self._run("--baseline", bl, "--l0", l0)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("coverage data missing", r.stdout)
        self.assertIn("[WARN]", r.stdout)

    # ===========================================================================
    # SCENARIO 12 — Both L0 AND L1 jobs fail
    # Coverage Gate does NOT trigger (workflow-level).  At script level, both
    # .info files are absent → both SKIP → exit 0 (harmless; gate is already
    # blocked at the workflow graph layer before the script is ever called).
    # ===========================================================================

    def test_s12_both_artifacts_absent(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        # Neither --l0 nor --l1 provided
        r = self._run("--baseline", bl)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        # Both rows + summary line show coverage data missing, OVERALL WARN
        self.assertGreaterEqual(r.stdout.count("coverage data missing"), 2)
        self.assertIn("[WARN]", r.stdout)
        self.assertIn("NOTE:", r.stdout)

    # ===========================================================================
    # SCENARIO 13 — Coverage Gate step itself throws an unexpected error
    # Expected: non-zero exit; error is visible; baseline NOT updated
    # (Simulated by passing a completely invalid path for --baseline that
    #  causes the argument parser or file logic to surface an error.)
    # ===========================================================================

    def test_s13_missing_required_baseline_arg(self):
        """Invoking the script without --baseline must fail (argparse error)."""
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--l0", l0, "--l1", l1)
        # argparse exits with code 2 on missing required argument
        self.assertNotEqual(r.returncode, 0)
        self.assertTrue(len(r.stderr) > 0, "Error must appear on stderr")

    # ===========================================================================
    # Partial-failure cases: one suite fails, other passes
    # ===========================================================================

    def test_only_l0_fails_gate_warns(self):
        """L0 below threshold, L1 passes → overall [WARN] but exit 0."""
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 70)  # 70 % ✗
        l1 = self._lcov("l1.info", 100, 80)  # 80 % ✓
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        # L1 row still shows PASS
        self.assertIn("[PASS]", r.stdout)
        self.assertIn("[WARN]", r.stdout)

    def test_only_l1_fails_gate_warns(self):
        """L1 below threshold, L0 passes → overall [WARN] but exit 0."""
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)  # 80 % ✓
        l1 = self._lcov("l1.info", 100, 70)  # 70 % ✗
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)

    def test_only_l0_regresses_gate_warns(self):
        """L0 regresses below baseline (still above threshold), L1 passes → [WARN] exit 0."""
        bl = self._baseline({"L0": 85.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)  # 80 % < 85 % baseline ✗
        l1 = self._lcov("l1.info", 100, 80)  # 80 % >= 75 % baseline ✓
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)

    # ===========================================================================
    # First-time setup: empty baseline {} → threshold-only
    # ===========================================================================

    def test_first_time_setup_empty_baseline_passes(self):
        bl = self._baseline({})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_first_time_setup_empty_baseline_below_threshold_warns(self):
        bl = self._baseline({})
        l0 = self._lcov("l0.info", 100, 70)
        l1 = self._lcov("l1.info", 100, 70)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        # Informational only — exit 0 even below threshold; [WARN] shown
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)

    # ===========================================================================
    # Output format validation
    # ===========================================================================

    def test_overall_pass_token_in_output(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertIn("OVERALL:", r.stdout)
        self.assertIn("[PASS]", r.stdout)

    def test_overall_warn_token_in_output(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 70)
        l1 = self._lcov("l1.info", 100, 70)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertIn("OVERALL:", r.stdout)
        self.assertIn("[WARN]", r.stdout)
        # Informational only — always exit 0
        self.assertEqual(r.returncode, 0)

    # ===========================================================================
    # --output-json baseline extraction
    # ===========================================================================

    def test_output_json_written_when_both_pass(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 82)
        out = os.path.join(self.tmp, "new-baseline.json")
        self._run(
            "--baseline", bl,
            "--l0", l0, "--l1", l1,
            "--output-json", out,
            "--commit", "abc123",
            "--timestamp", "2026-01-01T00:00:00Z",
        )
        self.assertTrue(os.path.isfile(out), "output-json must be written")
        with open(out) as fh:
            data = json.load(fh)
        self.assertEqual(data["L0"], 80.0)
        self.assertEqual(data["L1"], 82.0)
        self.assertEqual(data["commit"], "abc123")
        self.assertEqual(data["timestamp"], "2026-01-01T00:00:00Z")

    def test_output_json_written_even_when_gate_fails(self):
        """
        --output-json is written as long as coverage data is available,
        regardless of gate outcome.  The update-baseline step checks
        `if [ ! -s new-baseline.json ]` separately.
        """
        bl = self._baseline({"L0": 90.0, "L1": 90.0})
        l0 = self._lcov("l0.info", 100, 80)  # 80 % < 90 % baseline → WARN
        l1 = self._lcov("l1.info", 100, 80)
        out = os.path.join(self.tmp, "new-baseline-fail.json")
        r = self._run(
            "--baseline", bl,
            "--l0", l0, "--l1", l1,
            "--output-json", out,
        )
        # Informational only — always exit 0 regardless of gate outcome
        self.assertEqual(r.returncode, 0)
        self.assertTrue(os.path.isfile(out), "output-json written even on gate warning")

    def test_output_json_not_written_when_l0_missing(self):
        """When --l0 is explicitly requested but its .info file is absent, --output-json must NOT be written."""
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l1 = self._lcov("l1.info", 100, 82)
        out = os.path.join(self.tmp, "new-baseline-no-l0.json")
        r = self._run("--baseline", bl, "--l0", "/nonexistent/l0.info", "--l1", l1, "--output-json", out)
        self.assertFalse(os.path.isfile(out), "output-json must NOT be written when requested L0 data is absent")
        self.assertIn("WARNING", r.stderr)

    def test_output_json_not_written_when_l1_missing(self):
        """When --l1 is explicitly requested but its .info file is absent, --output-json must NOT be written."""
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)
        out = os.path.join(self.tmp, "new-baseline-no-l1.json")
        r = self._run("--baseline", bl, "--l0", l0, "--l1", "/nonexistent/l1.info", "--output-json", out)
        self.assertFalse(os.path.isfile(out), "output-json must NOT be written when requested L1 data is absent")
        self.assertIn("WARNING", r.stderr)

    def test_output_json_not_written_when_both_missing(self):
        bl = self._baseline({"L0": 75.0, "L1": 75.0})
        out = os.path.join(self.tmp, "new-baseline-neither.json")
        r = self._run("--baseline", bl, "--output-json", out)
        self.assertFalse(os.path.isfile(out))
        self.assertIn("WARNING", r.stderr)

    def test_output_json_written_with_l1_only(self):
        """When only --l1 is provided (no --l0), --output-json is written with just the L1 key.
        This is the standard case for repos that have no L0 tests (e.g. Dobby)."""
        bl = self._baseline({"L1": 75.0})
        l1 = self._lcov("l1.info", 100, 80)
        out = os.path.join(self.tmp, "new-baseline-l1-only.json")
        self._run(
            "--baseline", bl,
            "--l1", l1,
            "--output-json", out,
            "--commit", "abc123",
            "--timestamp", "2026-06-22T00:00:00Z",
        )
        self.assertTrue(os.path.isfile(out), "output-json must be written for L1-only repos")
        with open(out) as fh:
            data = json.load(fh)
        self.assertEqual(data["L1"], 80.0)
        self.assertNotIn("L0", data, "L0 key must not appear when --l0 was not passed")
        self.assertEqual(data["commit"], "abc123")

    # ===========================================================================
    # Baseline coercion: non-float L0/L1 values must not crash the script
    # (Fixes comment 2/7 — baseline.get("L0") not validated as float)
    # ===========================================================================

    def test_baseline_string_l0_treated_as_missing(self):
        """String value for L0 in baseline JSON → coerced to None → threshold-only."""
        bl = self._baseline({"L0": "not-a-number", "L1": 75.0})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        # Must not crash; L0 baseline treated as absent → threshold-only → pass
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_baseline_null_l1_treated_as_missing(self):
        """null value for L1 in baseline JSON → coerced to None → threshold-only."""
        bl = self._baseline({"L0": 80.0, "L1": None})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_baseline_both_non_float_threshold_only(self):
        """Both L0/L1 baseline values invalid → both threshold-only → pass if above 75%."""
        bl = self._baseline({"L0": "bad", "L1": "bad"})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)

    def test_baseline_both_non_float_below_threshold_warns(self):
        """Both L0/L1 baseline values invalid → threshold-only → [WARN] exit 0 if below 75%."""
        bl = self._baseline({"L0": "bad", "L1": "bad"})
        l0 = self._lcov("l0.info", 100, 70)
        l1 = self._lcov("l1.info", 100, 70)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("[WARN]", r.stdout)

    # ===========================================================================
    # _fmt_timestamp: null/non-string timestamp must not crash the report
    # (Fixes comment 8 — only ValueError was caught, not TypeError)
    # ===========================================================================

    def test_null_timestamp_in_baseline_does_not_crash(self):
        """null timestamp value in baseline JSON → TypeError handled → report still runs."""
        bl = self._baseline({"L0": 80.0, "L1": 80.0, "commit": "abc", "timestamp": None})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        # Must not crash; timestamp renders as fallback; gate passes
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)
        self.assertIn("OVERALL:", r.stdout)

    def test_integer_timestamp_in_baseline_does_not_crash(self):
        """Integer timestamp → TypeError in strptime → handled gracefully."""
        bl = self._baseline({"L0": 80.0, "L1": 80.0, "commit": "abc", "timestamp": 12345})
        l0 = self._lcov("l0.info", 100, 80)
        l1 = self._lcov("l1.info", 100, 80)
        r = self._run("--baseline", bl, "--l0", l0, "--l1", l1)
        self.assertEqual(r.returncode, 0, msg=r.stdout + r.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
