#!/usr/bin/env python3
"""Run GoogleTest binaries and report what actually executed.

Why this exists
---------------
`ctest` reports a GoogleTest binary as one test.  If that process aborts in the
middle (MSVC STL assertion, segfault, unhandled exception, ...) the remaining
cases never run, yet the only signal is a single red line.  A real example in
this repo: `RenderGraphIntegrationTest.TransientRT_...` indexed an empty
std::vector after a failed EXPECT, aborted the process, and silently swallowed
193 later integration cases (655/848 executed).

The second blind spot is `GTEST_SKIP`: a skipped case is reported by GoogleTest
but counted as a pass by ctest, so the public "N tests" figure overstates real
coverage.

This tool closes both by comparing the number of cases a binary *lists* against
the number that *actually ran*, and by surfacing every skip (with its reason).

Usage
-----
    python tools/audit/gtest_report.py path/to/dse_gtest_unit_tests.exe [...]
    python tools/audit/gtest_report.py --json tools/audit/gtest_report.json BIN...
    python tools/audit/gtest_report.py --max-skips 5 BIN...

Exit code is non-zero when a binary is incomplete, has failures, or exceeds the
skip budget.  Read-only, no third-party dependencies.
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

LABEL_PREFIX = "test-report:"


def _run(argv: list[str], cwd: Path | None) -> subprocess.CompletedProcess:
    return subprocess.run(
        argv,
        cwd=str(cwd) if cwd else None,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )


def list_cases(binary: Path, cwd: Path | None) -> list[str]:
    """Return the fully-qualified case names the binary advertises."""
    proc = _run([str(binary), "--gtest_list_tests"], cwd)
    if proc.returncode != 0:
        raise RuntimeError(
            f"{binary.name}: --gtest_list_tests failed with exit {proc.returncode}"
        )
    cases: list[str] = []
    suite = ""
    for line in proc.stdout.splitlines():
        stripped = line.strip()
        if not stripped:
            continue
        if line.startswith("  "):
            cases.append(f"{suite}{stripped}")
        elif stripped.endswith("."):
            suite = stripped
    return cases


def parse_junit(path: Path) -> dict:
    """Summarise a GoogleTest JUnit report.

    Returns observed / failed counts, the skip list, and the set of
    fully-qualified case names that really executed.
    """
    root = ET.parse(path).getroot()
    observed = 0
    failed = 0
    skipped = []
    executed = set()
    for case in root.iter("testcase"):
        observed += 1
        name = "%s.%s" % (case.get("classname", ""), case.get("name", ""))
        executed.add(name)
        if case.find("failure") is not None or case.find("error") is not None:
            failed += 1
        skip = case.find("skipped")
        if skip is not None:
            skipped.append((name, (skip.get("message") or "").strip()))
    return {"observed": observed, "failed": failed,
            "skipped": skipped, "executed": executed}


def report_binary(binary: Path, cwd: Path | None, workdir: Path) -> dict:
    result: dict = {"binary": binary.name, "path": str(binary)}
    listed = list_cases(binary, cwd)
    result["listed"] = len(listed)
    if not listed:
        result["error"] = "binary advertises zero tests"
        return result

    xml_path = workdir / f"{binary.stem}.xml"
    if xml_path.exists():
        xml_path.unlink()
    proc = _run(
        [str(binary), "--gtest_brief=1", f"--gtest_output=xml:{xml_path}"],
        cwd,
    )
    result["exit_code"] = proc.returncode
    result["output"] = proc.stdout

    if not xml_path.exists():
        # GoogleTest flushes the XML listener at the end of the run, so a missing
        # file means the process died before finishing.
        result["error"] = (
            "run produced no JUnit XML: the process aborted before completing "
            "(cases after the crash never executed)"
        )
        return result

    summary = parse_junit(xml_path)
    result.update(observed=summary["observed"], failed=summary["failed"],
                  skipped=summary["skipped"])
    result["missing"] = sorted(set(listed) - summary["executed"])
    result["incomplete"] = summary["observed"] < len(listed)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("binaries", nargs="+", help="GoogleTest executables to run")
    parser.add_argument("--cwd", default=".", help="working directory for the runs")
    parser.add_argument("--max-skips", type=int, default=None,
                        help="fail when a binary skips more than this many cases")
    parser.add_argument("--json", dest="json_out", default=None,
                        help="write the machine-readable report here")
    parser.add_argument("--keep-output", action="store_true",
                        help="echo each binary's raw output")
    parser.add_argument("--xml-dir", default=None,
                        help="keep the per-binary JUnit XML here (for CI artifacts)")
    args = parser.parse_args()

    cwd = Path(args.cwd).resolve()
    failures: list[str] = []
    reports: list[dict] = []

    kept: Path | None = Path(args.xml_dir).resolve() if args.xml_dir else None
    if kept is not None:
        kept.mkdir(parents=True, exist_ok=True)

    with tempfile.TemporaryDirectory(prefix="dse_gtest_report_") as tmp:
        workdir = Path(tmp)
        if kept is not None:
            workdir = kept
        for raw in args.binaries:
            binary = Path(raw)
            if not binary.is_absolute():
                binary = (cwd / binary).resolve()
            sys.stdout.write(f"{LABEL_PREFIX} running {binary.name}\n")
            try:
                rep = report_binary(binary, cwd, workdir)
            except Exception as exc:  # noqa: BLE001 - report, do not mask
                rep = {"binary": binary.name, "path": str(binary), "error": str(exc)}
            reports.append(rep)

            if args.keep_output and rep.get("output"):
                print(rep["output"])

            if "error" in rep:
                failures.append(f"{rep['binary']}: {rep['error']}")
                print(f"{LABEL_PREFIX} {rep['binary']}: ERROR - {rep['error']}")
                continue

            skipped = rep.get("skipped", [])
            print(
                f"{LABEL_PREFIX} {rep['binary']}: listed={rep['listed']} "
                f"ran={rep['observed']} failed={rep['failed']} skipped={len(skipped)} "
                f"exit={rep['exit_code']}"
            )
            for name, reason in skipped:
                print(f"{LABEL_PREFIX}   SKIP {name} :: {reason or '<no reason given>'}")
            if rep.get("incomplete"):
                failures.append(
                    f"{rep['binary']}: only {rep['observed']}/{rep['listed']} listed cases ran "
                    f"({rep['listed'] - rep['observed']} never executed)"
                )
            if rep["failed"]:
                failures.append(f"{rep['binary']}: {rep['failed']} failing case(s)")
            if args.max_skips is not None and len(skipped) > args.max_skips:
                failures.append(
                    f"{rep['binary']}: {len(skipped)} skipped cases exceed budget "
                    f"{args.max_skips}"
                )

    if args.json_out:
        Path(args.json_out).parent.mkdir(parents=True, exist_ok=True)
        Path(args.json_out).write_text(
            json.dumps({"reports": reports}, ensure_ascii=False, indent=2) + "\n",
            encoding="utf-8",
        )
        print(f"{LABEL_PREFIX} wrote {args.json_out}")

    if failures:
        print(f"{LABEL_PREFIX} FAILED", file=sys.stderr)
        for item in failures:
            print(f"{LABEL_PREFIX}  - {item}", file=sys.stderr)
        return 1
    print(f"{LABEL_PREFIX} OK: every listed case executed; skip budget respected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
