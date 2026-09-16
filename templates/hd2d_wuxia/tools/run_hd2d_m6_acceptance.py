#!/usr/bin/env python3
"""Reproducible HD-2D M6 acceptance runner.

Runs `_hd2d_m6_acceptance_test.lua` on the requested RHI backends, captures
screenshots into docs/design/hd2d_m6_shots/, verifies exit codes and then
invokes hd2d_pixel_stats.py for pixel/PSNR/SSIM evidence.

Examples:
  python run_hd2d_m6_acceptance.py
  python run_hd2d_m6_acceptance.py --backends opengl,vulkan,d3d11
  python run_hd2d_m6_acceptance.py --backends d3d11 --headless-d3d11
"""
from __future__ import annotations

import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SHOT_DIR = ROOT / "docs" / "design" / "hd2d_m6_shots"
STATS = ROOT / "templates" / "hd2d_wuxia" / "tools" / "hd2d_pixel_stats.py"
SCRIPT = "templates/hd2d_wuxia/scripts/_hd2d_m6_acceptance_test.lua"


def run_backend(binary: Path, backend: str, headless: bool, max_frames: int, shot_frame: int) -> Path:
    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_MAX_FRAMES"] = str(max_frames)
    env["DSE_SCREENSHOT_FRAME"] = str(shot_frame)
    env["DSE_SCREENSHOT_PATH"] = str(SHOT_DIR / (f"m6_{backend}_headless.png" if headless else f"m6_{backend}.png"))
    env.pop("DSE_RENDER_PIPELINE_PROFILE", None)
    env.pop("DSE_M6_ORTHO", None)
    if headless:
        env["DSE_DX11_HEADLESS"] = "1"
    else:
        env.pop("DSE_DX11_HEADLESS", None)

    cmd = [str(binary), f"--script={SCRIPT}"]
    print(f"[m6] run backend={backend} headless={headless} -> {env['DSE_SCREENSHOT_PATH']}")
    proc = subprocess.run(cmd, cwd=ROOT, env=env, text=True, encoding="utf-8", errors="replace", capture_output=True, timeout=180)
    if proc.returncode != 0:
        raise RuntimeError(f"{backend} exit={proc.returncode}\nstdout:\n{proc.stdout}\nstderr:\n{proc.stderr}")
    shot = Path(env["DSE_SCREENSHOT_PATH"])
    if not shot.exists():
        raise RuntimeError(f"{backend} did not write screenshot: {shot}")
    return shot


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--binary", default="bin/dsengine_lua_debug.exe")
    parser.add_argument("--backends", default="opengl,vulkan,d3d11")
    parser.add_argument("--headless-d3d11", action="store_true")
    parser.add_argument("--max-frames", type=int, default=18)
    parser.add_argument("--shot-frame", type=int, default=15)
    args = parser.parse_args()

    binary = (ROOT / args.binary).resolve()
    if not binary.exists():
        print(f"missing binary: {binary}", file=sys.stderr)
        return 2
    SHOT_DIR.mkdir(parents=True, exist_ok=True)

    shots: list[Path] = []
    for backend in [b.strip() for b in args.backends.split(",") if b.strip()]:
        shots.append(run_backend(binary, backend, False, args.max_frames, args.shot_frame))
        if backend == "d3d11" and args.headless_d3d11:
            shots.append(run_backend(binary, backend, True, args.max_frames, args.shot_frame))

    stats_cmd = [sys.executable, str(STATS)] + [str(p) for p in shots]
    subprocess.run(stats_cmd, cwd=ROOT, check=True)
    print("[m6] acceptance runner PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
