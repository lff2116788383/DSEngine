#!/usr/bin/env python3
"""Run phase5 tests with correct path."""
import os
import re
import sys

REPO_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
TEST_FILE = os.path.join(REPO_ROOT, "tests", "gtest", "unit", "engine",
                         "scripting", "dse_api_bindings_test.cpp")

# Extract NEW_TESTS from phase5_tests.py
P5_FILE = os.path.join(REPO_ROOT, "scripts", "phase5_tests.py")
with open(P5_FILE, "r", encoding="utf-8") as f:
    src = f.read()

idx = src.find("NEW_TESTS = r'''")
if idx >= 0:
    start = idx + len("NEW_TESTS = r'''")
    end = src.index("'''", start)
    new_tests = src[start:end]
else:
    print("ERROR: could not find NEW_TESTS in phase5_tests.py")
    sys.exit(1)

with open(TEST_FILE, "r", encoding="utf-8") as f:
    content = f.read()

if "Phase 5: Extended Coverage" not in content:
    content = content.rstrip() + "\n" + new_tests + "\n"
    with open(TEST_FILE, "w", encoding="utf-8") as f:
        f.write(content)
    print("Phase5 base tests added")
else:
    print("Phase5 base tests already present")

count = len(re.findall(r"TEST_F\(", content))
print(f"Total after phase5 base: {count}")
