"""编辑器冒烟截图测试

用法:
    python tools/editor_smoke_test.py --baseline         # 生成基准截图
    python tools/editor_smoke_test.py --compare          # 回归对比
    python tools/editor_smoke_test.py --compare --threshold=15

依赖: Python 3.9+ / Pillow (pip install Pillow)

流程:
  1. 启动 dsengine-editor.exe --headless --max-frames=60 --screenshot=<path>
  2. 等待进程退出
  3. 验证截图文件存在且尺寸合理
  4. (--compare) 与基准截图做像素 RMSE 对比
"""

import argparse
import math
import os
import pathlib
import subprocess
import sys
import time

# Windows stdout 编码修复
if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")
    for stream in (sys.stdout, sys.stderr):
        if stream and hasattr(stream, "reconfigure"):
            try:
                stream.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass

ROOT = pathlib.Path(__file__).resolve().parent.parent
BIN_DIR = ROOT / "bin"
BASELINE_DIR = ROOT / "tests" / "editor_smoke_baselines"
SCREENSHOT_DIR = ROOT / "tests" / "editor_smoke_current"
FIXTURES_DIR = ROOT / "tests" / "editor_smoke_fixtures"

# 测试场景列表：(名称, 额外参数)
SMOKE_CASES = [
    ("empty_scene", []),
    ("with_entity", [f"--scene={FIXTURES_DIR / 'with_entity.dscene'}"]),
    ("with_directional_light", [f"--scene={FIXTURES_DIR / 'with_directional_light.dscene'}"]),
]


def find_editor_exe() -> pathlib.Path | None:
    candidates = [
        BIN_DIR / "dsengine-editor.exe",
        BIN_DIR / "Debug" / "dsengine-editor.exe",
        BIN_DIR / "Release" / "dsengine-editor.exe",
        BIN_DIR / "RelWithDebInfo" / "dsengine-editor.exe",
    ]
    for c in candidates:
        if c.exists():
            return c
    return None


def run_editor_screenshot(
    exe: pathlib.Path,
    name: str,
    out_dir: pathlib.Path,
    extra_args: list[str],
    max_frames: int = 60,
    timeout: int = 60,
) -> tuple[bool, str, pathlib.Path]:
    """运行编辑器并截图，返回 (成功, 信息, 截图路径)"""
    out_dir.mkdir(parents=True, exist_ok=True)
    ss_path = out_dir / f"{name}.png"

    # 删除旧截图
    if ss_path.exists():
        ss_path.unlink()

    cmd = [
        str(exe),
        "--headless",
        f"--max-frames={max_frames}",
        f"--screenshot={ss_path}",
    ] + extra_args

    print(f"  CMD: {' '.join(cmd)}", flush=True)

    try:
        proc = subprocess.run(
            cmd,
            cwd=str(BIN_DIR),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            timeout=timeout,
        )
        if proc.returncode != 0:
            snippet = (proc.stdout or "")[-400:]
            return False, f"exit_code={proc.returncode} [{snippet.strip()[-200:]}]", ss_path
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT", ss_path
    except Exception as exc:
        return False, f"EXCEPTION: {exc}", ss_path

    if not ss_path.exists():
        return False, "screenshot_not_generated", ss_path
    size = ss_path.stat().st_size
    if size < 512:
        return False, f"screenshot_too_small ({size}B)", ss_path

    return True, f"ok ({size} bytes)", ss_path


def compute_rmse(img_a_path: pathlib.Path, img_b_path: pathlib.Path) -> float:
    """计算两张图片的像素 RMSE"""
    try:
        from PIL import Image
        import numpy as np
    except ImportError:
        print("WARNING: Pillow/numpy not installed, skipping pixel comparison")
        return -1.0

    a = np.array(Image.open(img_a_path).convert("RGB"), dtype=np.float64)
    b = np.array(Image.open(img_b_path).convert("RGB"), dtype=np.float64)

    if a.shape != b.shape:
        return 999.0

    mse = np.mean((a - b) ** 2)
    return math.sqrt(mse)


def cmd_baseline(args):
    exe = find_editor_exe()
    if not exe:
        print("ERROR: dsengine-editor.exe not found in bin/")
        return 1

    print(f"Editor: {exe}")
    print(f"Baseline dir: {BASELINE_DIR}\n")

    failed = 0
    for name, extra in SMOKE_CASES:
        print(f"[BASELINE] {name}")
        ok, info, ss = run_editor_screenshot(
            exe, name, BASELINE_DIR, extra, max_frames=args.max_frames
        )
        if ok:
            print(f"  OK: {info}")
        else:
            print(f"  FAIL: {info}")
            failed += 1

    print(f"\n{'='*60}")
    print(f"Baseline: {len(SMOKE_CASES)} cases, {failed} failed")
    return 1 if failed else 0


def cmd_compare(args):
    exe = find_editor_exe()
    if not exe:
        print("ERROR: dsengine-editor.exe not found in bin/")
        return 1

    print(f"Editor: {exe}")
    print(f"Baseline dir: {BASELINE_DIR}")
    print(f"Current dir:  {SCREENSHOT_DIR}")
    print(f"Threshold:    {args.threshold}\n")

    failed = 0
    for name, extra in SMOKE_CASES:
        print(f"[COMPARE] {name}")
        baseline_path = BASELINE_DIR / f"{name}.png"
        if not baseline_path.exists():
            print(f"  SKIP: no baseline (run --baseline first)")
            continue

        ok, info, ss = run_editor_screenshot(
            exe, name, SCREENSHOT_DIR, extra, max_frames=args.max_frames
        )
        if not ok:
            print(f"  FAIL (capture): {info}")
            failed += 1
            continue

        rmse = compute_rmse(baseline_path, ss)
        if rmse < 0:
            print(f"  OK (capture only, no pixel compare)")
        elif rmse <= args.threshold:
            print(f"  OK: RMSE={rmse:.2f} (threshold={args.threshold})")
        else:
            print(f"  FAIL: RMSE={rmse:.2f} > threshold={args.threshold}")
            failed += 1

    print(f"\n{'='*60}")
    print(f"Compare: {len(SMOKE_CASES)} cases, {failed} failed")
    return 1 if failed else 0


def main():
    parser = argparse.ArgumentParser(description="Editor smoke screenshot test")
    sub = parser.add_subparsers(dest="command")

    # 为兼容 --baseline / --compare 风格参数
    parser.add_argument("--baseline", action="store_true", help="Generate baseline screenshots")
    parser.add_argument("--compare", action="store_true", help="Compare against baseline")
    parser.add_argument("--threshold", type=float, default=15.0, help="RMSE threshold (default 15)")
    parser.add_argument("--max-frames", type=int, default=60, help="Frames to render (default 60)")

    args = parser.parse_args()

    if args.baseline:
        sys.exit(cmd_baseline(args))
    elif args.compare:
        sys.exit(cmd_compare(args))
    else:
        parser.print_help()
        print("\nUse --baseline to generate reference screenshots, or --compare to test.")
        sys.exit(1)


if __name__ == "__main__":
    main()
