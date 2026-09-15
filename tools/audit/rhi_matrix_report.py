#!/usr/bin/env python3
"""Turn a smoke-battery JUnit XML into a per-backend coverage / consistency matrix.

Why this exists
---------------
`platformMatrix` ("required desktop and web rendering backends behave consistently")
was previously evidenced by hand-running the smoke battery and eyeballing the log.
That cannot be a release gate: it is not machine-readable, and it silently tolerates
a backend dropping out of coverage entirely (e.g. a suite that skips on Vulkan).

This tool classifies every executed case by the backend(s) it references and prints
the matrix, plus the cross-backend consistency cases (the ones that compare two
backends' pixels).  It fails when a required backend has no coverage at all, so
"we ran the GPU battery" can never again mean "we might not have touched Vulkan".

Usage
-----
    python tools/audit/rhi_matrix_report.py --xml path/to/dse_gtest_smoke_tests.xml
    python tools/audit/rhi_matrix_report.py --xml ... --json out.json --backend opengl --backend vulkan
"""

from __future__ import annotations

import argparse
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

# Tokens that identify which backend a case exercises.  Matched case-insensitively
# against "<suite>.<case>".
BACKEND_TOKENS = {
    "opengl": ("opengl", "glrhi", "gl:", "_gl", "gl_"),
    "d3d11": ("d3d11", "dx11", "d3d"),
    "vulkan": ("vulkan", "vkrhi", "vk_"),
}
CROSS_TOKENS = ("crossbackend", "cross_backend", "gl-vs", "vs-d3d11", "vs-vulkan", "threerhi")


def classify(name: str) -> tuple[list[str], bool]:
    low = name.lower()
    backends = [b for b, toks in BACKEND_TOKENS.items() if any(t in low for t in toks)]
    cross = any(t in low for t in CROSS_TOKENS)
    return backends, cross


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xml", required=True, help="JUnit XML produced by the smoke battery")
    parser.add_argument("--json", dest="json_out", default=None)
    parser.add_argument("--backend", action="append", dest="required", default=None,
                        help="backend that must have coverage (repeatable; default opengl/d3d11/vulkan)")
    args = parser.parse_args()

    required = args.required or ["opengl", "d3d11", "vulkan"]
    xml_path = Path(args.xml)
    if not xml_path.is_file():
        print(f"rhi-matrix: XML not found: {xml_path}", file=sys.stderr)
        return 2

    root = ET.parse(xml_path).getroot()
    matrix: dict = {b: {"cases": 0, "passed": 0, "failed": 0, "skipped": 0, "names": []} for b in BACKEND_TOKENS}
    cross = {"cases": 0, "passed": 0, "failed": 0, "names": []}
    other = {"cases": 0, "passed": 0, "failed": 0}
    total = {"cases": 0, "passed": 0, "failed": 0, "skipped": 0}

    for case in root.iter("testcase"):
        name = f"{case.get('classname', '')}.{case.get('name', '')}"
        failed = case.find("failure") is not None or case.find("error") is not None
        skipped = case.find("skipped") is not None
        total["cases"] += 1
        total["failed"] += int(failed)
        total["skipped"] += int(skipped)
        total["passed"] += int(not failed and not skipped)

        is_cross = any(t in name.lower() for t in CROSS_TOKENS)
        if is_cross:
            cross["cases"] += 1
            cross["failed"] += int(failed)
            cross["passed"] += int(not failed and not skipped)
            cross["names"].append(name)
            continue
        backends, _ = classify(name)
        if backends:
            for b in backends:
                matrix[b]["cases"] += 1
                matrix[b]["passed"] += int(not failed and not skipped)
                matrix[b]["failed"] += int(failed)
                matrix[b]["skipped"] += int(skipped)
                matrix[b]["names"].append(name)
        else:
            other["cases"] += 1
            other["failed"] += int(failed)
            other["passed"] += int(not failed and not skipped)

    print(f"rhi-matrix: source {xml_path}")
    print(f"rhi-matrix: {'backend':<10} {'cases':>6} {'passed':>7} {'failed':>7} {'skipped':>8}")
    for b in sorted(matrix):
        m = matrix[b]
        flag = "" if m["cases"] > 0 else "   <-- NO COVERAGE"
        print(f"rhi-matrix: {b:<10} {m['cases']:>6} {m['passed']:>7} {m['failed']:>7} {m['skipped']:>8}{flag}")
    print(f"rhi-matrix: {'cross':<10} {cross['cases']:>6} {cross['passed']:>7} {cross['failed']:>7} {'-':>8}")
    print(f"rhi-matrix: {'other':<10} {other['cases']:>6} {other['passed']:>7} {other['failed']:>7} {'-':>8}")
    print(f"rhi-matrix: total {total['cases']} cases, {total['passed']} passed, "
          f"{total['failed']} failed, {total['skipped']} skipped")

    errors = []
    for b in required:
        if matrix.get(b, {}).get("cases", 0) == 0:
            errors.append(f"required backend '{b}' has zero executed cases (did it silently skip?)")
        if matrix.get(b, {}).get("failed", 0) > 0:
            errors.append(f"backend '{b}' has {matrix[b]['failed']} failing case(s)")
    if cross["failed"] > 0:
        errors.append(f"{cross['failed']} cross-backend consistency case(s) failed")

    if args.json_out:
        report = {
            "schemaVersion": 1,
            "source": str(xml_path),
            "requiredBackends": required,
            "matrix": {b: {k: v for k, v in m.items() if k != "names"} for b, m in matrix.items()},
            "crossBackend": {k: v for k, v in cross.items() if k != "names"},
            "crossBackendCases": cross["names"],
            "unclassified": other,
            "total": total,
            "errors": errors,
        }
        out = Path(args.json_out)
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"rhi-matrix: wrote {out}")

    if errors:
        print("rhi-matrix: FAILED", file=sys.stderr)
        for e in errors:
            print(f"rhi-matrix:  - {e}", file=sys.stderr)
        return 1
    print(f"rhi-matrix: OK - required backends all covered, cross-backend consistency cases green")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
