#!/usr/bin/env python3
"""CI 门禁：apps/editor_cpp/core/ 不得引用任何 UI 框架（ImGui / ImGuizmo）。

core/ 是编辑器命令/查询门面层，必须对 UI 框架零知识，以保证未来可替换 UI 框架。
任何把 imgui/ImGui/ImVec/ImGuizmo 泄漏进 core/ 的改动，本脚本退出码非 0。

用法:
    python scripts/check_core_ui_free.py
退出码: 0=干净, 1=发现违规, 2=core 目录缺失/无文件可扫。

与 EditorCoreIsolation gtest 同源（gtest 供 ctest，本脚本供 CI workflow / 本地）。
"""
import os
import sys

FORBIDDEN = ("imgui", "ImGui", "ImVec", "ImGuizmo", "imgui_impl")
SCAN_EXT = (".h", ".hpp", ".cpp", ".cc", ".inl")


def main() -> int:
    repo_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    core_dir = os.path.join(repo_root, "apps", "editor_cpp", "core")

    if not os.path.isdir(core_dir):
        print(f"[check_core_ui_free] ERROR: core dir not found: {core_dir}")
        return 2

    scanned = 0
    violations = []
    for root, _dirs, files in os.walk(core_dir):
        for fname in files:
            if not fname.endswith(SCAN_EXT):
                continue
            path = os.path.join(root, fname)
            scanned += 1
            with open(path, "r", encoding="utf-8", errors="replace") as fh:
                for lineno, line in enumerate(fh, 1):
                    for tok in FORBIDDEN:
                        if tok in line:
                            rel = os.path.relpath(path, repo_root)
                            violations.append(f"{rel}:{lineno}: forbidden token '{tok}'")

    if scanned == 0:
        print(f"[check_core_ui_free] ERROR: no source files scanned under {core_dir}")
        return 2

    if violations:
        print("[check_core_ui_free] FAIL: EditorCore must stay UI-framework free.")
        for v in violations:
            print(f"  - {v}")
        return 1

    print(f"[check_core_ui_free] OK: scanned {scanned} file(s), no UI-framework references.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
