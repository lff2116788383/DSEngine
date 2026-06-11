#!/usr/bin/env python3
"""Fix duplicate / low-quality gtest case names using git originals."""

from __future__ import annotations

import importlib.util
import re
import subprocess
import sys
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TESTS_DIR = ROOT / "tests"
RENAME_SCRIPT = ROOT / "tools" / "rename_gtest_cases_to_english.py"

TEST_RE = re.compile(r"TEST(?:_F|P)?\s*\(\s*([^,]+)\s*,\s*([^)]+)\s*\)")
TEST_MACRO = re.compile(r"(TEST(?:_F|P)?\s*\(\s*([^,]+)\s*,\s*)([^)]+?)(\s*\))")
HAS_CJK = re.compile(r"[\u4e00-\u9fff]")

MANUAL: dict[str, str] = {
    "未知方法返回MethodNotFound": "UnknownMethodReturnsMethodNotFound",
    "Ping_返回pong": "Ping_ReturnsPong",
}


def load_translator():
    spec = importlib.util.spec_from_file_location("rename_gtest", RENAME_SCRIPT)
    mod = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(mod)
    return mod.translate_case_name, mod.load_phrase_table()


def git_original(path: Path) -> str | None:
    rel = path.relative_to(ROOT).as_posix()
    try:
        return subprocess.check_output(
            ["git", "show", f"HEAD:{rel}"],
            cwd=ROOT,
            text=True,
            encoding="utf-8",
            errors="replace",
            stderr=subprocess.DEVNULL,
        )
    except subprocess.CalledProcessError:
        return None


def extract_cases(text: str) -> list[tuple[str, str]]:
    return [(m.group(1).strip(), m.group(2).strip()) for m in TEST_RE.finditer(text)]


def to_unique(base: str, used: set[str]) -> str:
    if base not in used:
        used.add(base)
        return base
    i = 2
    while f"{base}_{i}" in used:
        i += 1
    name = f"{base}_{i}"
    used.add(name)
    return name


def desired_name(orig_name: str, index: int, translate_case_name, table) -> str:
    if orig_name in MANUAL:
        name = MANUAL[orig_name]
    elif HAS_CJK.search(orig_name):
        name = translate_case_name(orig_name, table)
    else:
        name = orig_name

    if name in ("Case", "") or HAS_CJK.search(name):
        name = f"TestCase{index + 1}"
    return name


def rewrite_file_case_names(path: Path, new_names: list[str]) -> int:
    text = path.read_text(encoding="utf-8")
    idx = 0
    changed = 0

    def repl(match: re.Match[str]) -> str:
        nonlocal idx, changed
        old = match.group(3).strip()
        new = new_names[idx]
        idx += 1
        if old != new:
            changed += 1
            return f"{match.group(1)}{new}{match.group(4)}"
        return match.group(0)

    new_text = TEST_MACRO.sub(repl, text)
    if changed:
        path.write_text(new_text, encoding="utf-8", newline="\n")
    return changed


def main() -> int:
    translate_case_name, table = load_translator()
    total_changed = 0

    for path in sorted(TESTS_DIR.rglob("*.cpp")):
        rel = path.relative_to(ROOT)
        tracked = subprocess.run(
            ["git", "ls-files", "--error-unmatch", rel.as_posix()],
            cwd=ROOT,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        ).returncode == 0
        if not tracked:
            continue

        original_text = git_original(path)
        if original_text is None:
            continue

        orig_cases = extract_cases(original_text)
        cur_cases = extract_cases(path.read_text(encoding="utf-8"))
        if len(orig_cases) != len(cur_cases):
            print(f"WARN count mismatch: {rel}")
            continue

        used_by_suite: dict[str, set[str]] = {}
        new_names: list[str] = []
        for i, ((suite, cur_name), (_, orig_name)) in enumerate(zip(cur_cases, orig_cases)):
            used = used_by_suite.setdefault(suite, set())
            base = desired_name(orig_name, i, translate_case_name, table)
            unique = to_unique(base, used)
            new_names.append(unique)

        total_changed += rewrite_file_case_names(path, new_names)

    dup_count = 0
    cn_count = 0
    for path in sorted(TESTS_DIR.rglob("*.cpp")):
        text = path.read_text(encoding="utf-8")
        cases = extract_cases(text)
        for _, name in cases:
            if HAS_CJK.search(name):
                cn_count += 1
        for suite in {s for s, _ in cases}:
            names = [n for s, n in cases if s == suite]
            dup_count += len(names) - len(set(names))

    print(f"Updated {total_changed} case names.")
    print(f"Remaining Chinese case names: {cn_count}")
    print(f"Remaining duplicate names (count): {dup_count}")
    return 0 if cn_count == 0 and dup_count == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
