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
Coverage comparison script for Dobby.

Reads L1 (and optionally L0) lcov coverage results, compares overall line
coverage against the stored baseline from the build-metadata branch, and
prints a summary. Both --l0 and --l1 are optional; only suites explicitly
passed are evaluated.
"""

import argparse
import datetime
import json
import os
import sys
from typing import Optional


# Minimum threshold
THRESHOLD = 75.0

_GREEN = "\033[32m"
_RED   = "\033[31m"
_RESET = "\033[0m"

_SEP_WIDTH    = 64
_OVERALL_WIDTH = 80
_SEP    = "\u2500" * _SEP_WIDTH
_HEADER = "\u2500\u2500 Coverage Gate Report " + "\u2500" * (_SEP_WIDTH - 24)


def _colored(token: str, ok: bool) -> str:
    return f"{_GREEN if ok else _RED}{token}{_RESET}"


def _fmt_timestamp(ts: str) -> str:
    """Convert '2026-05-28T12:00:00Z' -> '2026-05-28 12:00 UTC'."""
    try:
        dt = datetime.datetime.strptime(ts, "%Y-%m-%dT%H:%M:%SZ")
        return dt.strftime("%Y-%m-%d %H:%M UTC")
    except (ValueError, TypeError):
        return ts


def _delta_str(current: float, baseline: float) -> str:
    delta = current - baseline
    sign = "+" if delta >= 0 else ""
    return f"{sign}{delta:.2f}%"


def _join_names(names: list) -> str:
    return names[0] if len(names) == 1 else " and ".join(names)


def _suite_analysis(current: Optional[float], baseline: Optional[float]):
    """Analyse one test suite.

    Returns (ok, result_str, delta_disp, warn_reason):
      ok          - True when no advisory issues found.
      result_str  - Coloured [PASS]/[WARN] token + detail for the table.
      delta_disp  - String for the Delta column ("N/A" when skipped).
      warn_reason - Reason phrase for the summary line; None when ok.
    """
    if current is None:
        reason = "coverage data missing"
        return False, f"{_colored('[WARN]', False)}  {reason}", "N/A", reason

    threshold_ok = current >= THRESHOLD

    if baseline is None:
        # No baseline stored — threshold check only.
        regression_ok = True
        detail        = "" if threshold_ok else "below threshold"
        delta_disp    = "N/A"
    elif baseline == 0.0:
        # A zero baseline is unreliable — skip regression check.
        regression_ok = True
        base_note  = "baseline unreliable (0%) \u00b7 delta skipped"
        detail     = f"below threshold \u00b7 {base_note}" if not threshold_ok else base_note
        delta_disp = "N/A"
    else:
        regression_ok = current >= baseline
        delta_disp    = _delta_str(current, baseline)
        if threshold_ok and regression_ok:
            detail = ""
        elif not threshold_ok and not regression_ok:
            detail = "below threshold \u00b7 dropped from baseline"
        elif not threshold_ok:
            detail = "below threshold \u00b7 no baseline regression"
        else:
            detail = "above threshold but dropped from baseline"

    overall_ok  = threshold_ok and regression_ok
    token       = _colored("[PASS]", True) if overall_ok else _colored("[WARN]", False)
    result_str  = f"{token}  {detail}" if detail else token
    warn_reason = detail if not overall_ok else None
    return overall_ok, result_str, delta_disp, warn_reason


def _build_summary(warn_suites: list) -> str:
    """Build a compact summary from WARN suite (name, reason) pairs."""
    if not warn_suites:
        return ""
    groups: dict = {}
    for name, reason in warn_suites:
        groups.setdefault(reason, []).append(name)
    parts = [f"{_join_names(names)} {reason}" for reason, names in groups.items()]
    return ". ".join(parts)


# lcov parsing
def parse_lcov_coverage(path: str) -> Optional[float]:
    """Return overall line coverage % from an lcov .info file, or None.

    An lcov .info file contains per-source-file records separated by
    ``end_of_record``.  Each record may include:
      LF:<lines found>   — total instrumented lines in that file
      LH:<lines hit>     — lines executed at least once

    We aggregate across all records to produce a single project-wide %.
    Returns None when the file is absent, empty, or contains no line data.
    """
    if not path or not os.path.isfile(path):
        return None

    total_found = 0
    total_hit = 0

    try:
        with open(path, "r", encoding="utf-8", errors="replace") as fh:
            for raw in fh:
                line = raw.strip()
                if line.startswith("LF:"):
                    try:
                        total_found += int(line[3:])
                    except ValueError:
                        pass
                elif line.startswith("LH:"):
                    try:
                        total_hit += int(line[3:])
                    except ValueError:
                        pass
    except OSError as exc:
        print(f"  WARNING: Could not read {path}: {exc}", file=sys.stderr)
        return None

    if total_found == 0:
        return None

    return round((total_hit / total_found) * 100.0, 2)


def _coerce_pct(value: object) -> Optional[float]:
    """Coerce a baseline percentage value to float, or None if invalid."""
    if value is None:
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


# Baseline loading
def load_baseline(path: str) -> dict:
    """Load baseline JSON; return an empty dict on any error."""
    if not path or not os.path.isfile(path):
        return {}
    try:
        with open(path, "r", encoding="utf-8") as fh:
            data = json.load(fh)
        if isinstance(data, dict):
            return data
        print(
            f"  WARNING: Baseline {path} is not a JSON object (got {type(data).__name__}) — ignoring",
            file=sys.stderr,
        )
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"  WARNING: Could not parse baseline {path}: {exc}", file=sys.stderr)
    return {}



def main() -> None:
    parser = argparse.ArgumentParser(
        description=(
            "Compare L0/L1 coverage against the develop baseline.\n"
            "Exits 1 when coverage fails threshold or regresses from baseline."
        )
    )
    parser.add_argument("--baseline", required=True, metavar="PATH",
                        help="Path to coverage-baseline.json.")
    parser.add_argument("--l0", required=False, metavar="PATH",
                        help="Path to the L0 lcov filtered_coverage.info file.")
    parser.add_argument("--l1", required=False, metavar="PATH",
                        help="Path to the L1 lcov filtered_coverage.info file.")
    parser.add_argument("--output-json", required=False, metavar="PATH",
                        help="Write {L0, L1, commit, timestamp} JSON here for baseline update.")
    parser.add_argument("--commit", required=False, default="",
                        help="Commit SHA to embed in --output-json.")
    parser.add_argument("--timestamp", required=False, default="",
                        help="ISO 8601 timestamp to embed in --output-json.")
    args = parser.parse_args()

    baseline    = load_baseline(args.baseline)

    baseline_l0: Optional[float] = _coerce_pct(baseline.get("L0"))
    baseline_l1: Optional[float] = _coerce_pct(baseline.get("L1"))

    l0_coverage = parse_lcov_coverage(args.l0) if args.l0 else None
    l1_coverage = parse_lcov_coverage(args.l1) if args.l1 else None

    # ------------------------------------------------------------------
    # Optional: write extracted numbers for baseline update.
    # Skipped when a *requested* suite (explicitly passed as an arg) produced
    # no data — indicates a broken artifact.  Suites not passed as args are
    # simply omitted from the payload (repos may track L1 only or L0 only).
    # ------------------------------------------------------------------
    if args.output_json:
        l0_failed = args.l0 is not None and l0_coverage is None
        l1_failed = args.l1 is not None and l1_coverage is None
        has_data  = l0_coverage is not None or l1_coverage is not None
        if not l0_failed and not l1_failed and has_data:
            payload: dict = {"commit": args.commit or "", "timestamp": args.timestamp or ""}
            if l0_coverage is not None:
                payload["L0"] = l0_coverage
            if l1_coverage is not None:
                payload["L1"] = l1_coverage
            try:
                with open(args.output_json, "w", encoding="utf-8") as fh:
                    json.dump(payload, fh, indent=2)
                    fh.write("\n")
            except OSError as exc:
                print(f"  WARNING: Could not write {args.output_json}: {exc}", file=sys.stderr)
        else:
            print(
                f"  WARNING: --output-json skipped: coverage data incomplete "
                f"(L0={l0_coverage}, L1={l1_coverage})",
                file=sys.stderr,
            )

    l0_ok, l0_result, l0_delta, l0_reason = _suite_analysis(l0_coverage, baseline_l0)
    l1_ok, l1_result, l1_delta, l1_reason = _suite_analysis(l1_coverage, baseline_l1)

    all_ok       = l0_ok and l1_ok
    status_token = _colored("[PASS]", True) if all_ok else _colored("[WARN]", False)

    # Output report
    print()
    print(_HEADER)
    if baseline:
        commit = baseline.get("commit", "unknown")
        ts     = _fmt_timestamp(baseline.get("timestamp", ""))
        print(f"  Baseline   {commit}  ({ts})")
    else:
        print("  Baseline   N/A  (first-time setup \u2014 regression check skipped)")
    print(f"  Threshold  {THRESHOLD}%  |  Status  {status_token}  (informational \u2014 PRs are not blocked)")
    print(_SEP)

    # Coverage table
    print(f"  {'Suite':<7}{'Current':<9}{'Baseline':<10}{'Delta':<10}Result")
    for name, current, base, result, delta_disp in [
        ("L0", l0_coverage, baseline_l0, l0_result, l0_delta),
        ("L1", l1_coverage, baseline_l1, l1_result, l1_delta),
    ]:
        cur_str  = f"{current:.2f}%" if current is not None else "N/A"
        base_str = f"{base:.2f}%"    if base    is not None else "N/A"
        print(f"  {name:<7}{cur_str:<9}{base_str:<10}{delta_disp:<10}{result}")

    print(_SEP)

    # Summary + overall bar
    warn_suites = [(n, r) for n, r in [("L0", l0_reason), ("L1", l1_reason)] if r]
    summary = _build_summary(warn_suites)
    if summary:
        print(f"  {summary}")

    # Notify when one or both suites had no coverage data (artifact absent).
    # Gate logic is unchanged — SKIP is treated as passing by design.
    skipped = [n for n, cov in [("L0", l0_coverage), ("L1", l1_coverage)] if cov is None]
    if skipped:
        print(f"  NOTE: {_join_names(skipped)} coverage data absent \u2014 artifact missing or unreadable.")

    # " OVERALL: [PASS/WARN] " = 1 + 9 + 6 + 1 = 17 visible chars
    # left + " OVERALL: " + token(6) + " " + right == _OVERALL_WIDTH
    _mid = len(" OVERALL: ") + 6 + len(" ")  # 17
    left  = "\u2500" * ((_OVERALL_WIDTH - _mid) // 2)          # 31
    right = "\u2500" * (_OVERALL_WIDTH - _mid - len(left))     # 32
    print(f"{left} OVERALL: {status_token} {right}")
    print()

    # Informational only — always exit 0 so PRs are never blocked.
    sys.exit(0)


if __name__ == "__main__":
    main()
