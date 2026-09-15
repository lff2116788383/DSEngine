#!/usr/bin/env python3
"""Ratchet on repository size: tracked bytes may only go down, never up silently.

Why this exists
---------------
The working tree is ~373 MiB with prebuilt third-party binaries (.lib/.dll) and large
art assets committed straight into Git; .git is ~250 MiB.  That cost lands on every
clone and every CI cache, and it grows one "just this once" commit at a time.  A
history rewrite is out of scope, but the growth can be stopped immediately: record
the current size as a baseline and fail whenever a change makes it bigger, or adds a
single file above the per-file cap that is not grandfathered.

Usage
-----
    python tools/audit/repo_size_budget.py                 # check (exit 1 on growth)
    python tools/audit/repo_size_budget.py --json out.json
    python tools/audit/repo_size_budget.py --update-baseline          # ratchet down
    python tools/audit/repo_size_budget.py --update-baseline --force  # allow growth (explicit)
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "tools" / "audit" / "repo_size_baseline.json"
MIB = 1024 * 1024

# 每文件上限：超过它的新文件必须走 Git LFS / 外部依赖，或显式 --force 说明理由。
# 8 MiB 是"明显是大二进制而非源码"的经验线；基线建立时已存在的超额文件会被
# 登记为 grandfathered（只许减少，不许新增）。
DEFAULT_MAX_FILE_BYTES = 8 * MIB
# 总预算余量：源码/文档正常增长不应该被门禁误杀，留 4 MiB（约十万行源码量级）；
# 真正的大块增长（预编译库、音频/纹理）仍会立刻触发失败。
DEFAULT_SLACK_BYTES = 4 * MIB


def tracked_files() -> list[str]:
    out = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True).stdout
    return [f for f in out.decode("utf-8", "replace").split("\0") if f]


def measure() -> tuple[int, list[tuple[int, str]]]:
    rows: list[tuple[int, str]] = []
    total = 0
    for rel in tracked_files():
        path = ROOT / rel
        try:
            size = path.stat().st_size
        except OSError:
            continue
        total += size
        rows.append((size, rel))
    rows.sort(reverse=True)
    return total, rows


def load_baseline() -> dict:
    if not BASELINE.is_file():
        return {}
    return json.loads(BASELINE.read_text(encoding="utf-8"))


def write_baseline(total: int, rows: list[tuple[int, str]], max_file_bytes: int,
                   slack_bytes: int = DEFAULT_SLACK_BYTES) -> None:
    data = {
        "schemaVersion": 1,
        "comment": "Ratchet for repo size. maxFileBytes caps any single file (new files above it "
                   "must go to Git LFS / an external dependency); grandfathered lists files that "
                   "already exceeded the cap when the baseline was taken (may only shrink). "
                   "totalBytes + totalSlackBytes is the hard ceiling for the whole tracked tree. "
                   "Tightening is free; loosening needs an explicit --force.",
        "totalBytes": total,
        "totalSlackBytes": slack_bytes,
        "maxFileBytes": max_file_bytes,
        "grandfathered": sorted(rel for size, rel in rows if size > max_file_bytes),
        "grandfatheredSizes": [{"path": rel, "bytes": size}
                               for size, rel in sorted(rows, key=lambda r: r[1])
                               if size > max_file_bytes],
    }
    BASELINE.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"repo-size: baseline written: total {total/MIB:.1f} MiB (+{slack_bytes/MIB:.1f} MiB slack), "
          f"maxFile {max_file_bytes/MIB:.1f} MiB, grandfathered {len(data['grandfathered'])} file(s)")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--json", dest="json_out", default=None)
    parser.add_argument("--update-baseline", action="store_true")
    parser.add_argument("--force", action="store_true", help="with --update-baseline: allow the total to grow")
    parser.add_argument("--top", type=int, default=10, help="how many largest files to print")
    args = parser.parse_args()

    total, rows = measure()
    baseline = load_baseline()

    if args.update_baseline:
        if baseline:
            old_total = int(baseline.get("totalBytes", 0))
            old_slack = int(baseline.get("totalSlackBytes", DEFAULT_SLACK_BYTES))
            old_cap = int(baseline.get("maxFileBytes", DEFAULT_MAX_FILE_BYTES))
            if total > old_total and not args.force:
                print(f"repo-size: refusing to raise the ratchet "
                      f"({old_total/MIB:.1f} -> {total/MIB:.1f} MiB); pass --force to allow growth",
                      file=sys.stderr)
                return 1
            cap = old_cap if old_cap else DEFAULT_MAX_FILE_BYTES
            write_baseline(total, rows, cap, old_slack)
            return 0
        write_baseline(total, rows, DEFAULT_MAX_FILE_BYTES, DEFAULT_SLACK_BYTES)
        return 0

    print(f"repo-size: {len(rows)} tracked files, {total/MIB:.1f} MiB total")
    for size, rel in rows[: max(0, args.top)]:
        print(f"repo-size:   {size/MIB:8.2f} MiB  {rel}")

    if not baseline:
        print("repo-size: no baseline yet - run --update-baseline to record the current size",
              file=sys.stderr)
        return 2

    cap = int(baseline.get("maxFileBytes", DEFAULT_MAX_FILE_BYTES))
    slack = int(baseline.get("totalSlackBytes", DEFAULT_SLACK_BYTES))
    grandfathered = set(baseline.get("grandfathered", []))
    budget_total = int(baseline.get("totalBytes", 0))
    ceiling = budget_total + slack

    over_cap = [(s, r) for s, r in rows if s > cap and r not in grandfathered]
    errors = []
    if total > ceiling:
        errors.append(f"tracked size grew past the ceiling: {total/MIB:.1f} MiB > "
                      f"{ceiling/MIB:.1f} MiB (baseline {budget_total/MIB:.1f} + slack {slack/MIB:.1f})")
    # a grandfathered file must not grow further either
    old_by_path = {e.get("path"): e.get("bytes", 0) for e in baseline.get("grandfatheredSizes", [])}
    for size, rel in rows:
        if rel in grandfathered and rel in old_by_path and size > old_by_path[rel]:
            errors.append(f"grandfathered large file grew: {rel} "
                          f"({old_by_path[rel]/MIB:.2f} -> {size/MIB:.2f} MiB)")
    for size, rel in over_cap:
        errors.append(f"new file above the {cap/MIB:.1f} MiB per-file cap: {rel} ({size/MIB:.1f} MiB) "
                      f"- use Git LFS / an external dependency, or justify it via --update-baseline --force")

    if args.json_out:
        report = {
            "schemaVersion": 1,
            "trackedFiles": len(rows),
            "totalBytes": total,
            "baselineTotalBytes": budget_total,
            "totalCeilingBytes": ceiling,
            "maxFileBytes": cap,
            "largest": [{"path": r, "bytes": s} for s, r in rows[: max(1, args.top)]],
            "grandfatheredCount": len(grandfathered),
            "errors": errors,
        }
        Path(args.json_out).write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
        print(f"repo-size: wrote {args.json_out}")

    if errors:
        print("repo-size: FAILED", file=sys.stderr)
        for e in errors:
            print(f"repo-size:  - {e}", file=sys.stderr)
        return 1

    print(f"repo-size: OK - {total/MIB:.1f} MiB <= ceiling {ceiling/MIB:.1f} MiB "
          f"({(ceiling-total)/MIB:.1f} MiB headroom); per-file cap {cap/MIB:.1f} MiB respected "
          f"({len(grandfathered)} grandfathered file(s) may only shrink)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
