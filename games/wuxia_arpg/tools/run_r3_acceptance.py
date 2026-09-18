#!/usr/bin/env python3
"""R3 acceptance for the new games/wuxia_arpg project (no template dependency)."""
from __future__ import annotations
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BINARY = ROOT / "bin" / "dsengine_lua_debug.exe"
STATS = ROOT / "games" / "wuxia_arpg" / "tools" / "pixel_stats.py"
LOGIC = "games/wuxia_arpg/scripts/_logic_test.lua"
GAME = "games/wuxia_arpg/scripts/main.lua"


def run(cmd, env, timeout=240):
    print("[r3] run:", " ".join(cmd))
    p = subprocess.run(cmd, cwd=ROOT, env=env, text=True, encoding="utf-8",
                       errors="replace", capture_output=True, timeout=timeout)
    if p.returncode != 0:
        raise RuntimeError(f"exit={p.returncode}\n{p.stdout}\n{p.stderr}")
    return p


def parse_luma(out: str):
    d = {}
    for m in re.finditer(r"\[pixel\] (.+?) .*?mean_luma=([0-9.]+)", out):
        d[m.group(1)] = float(m.group(2))
    return d


def run_logic():
    env = os.environ.copy()
    env["DSE_WUXIA_ACCEPT"] = "1"
    p = run([str(BINARY), f"--script={LOGIC}"], env, timeout=60)
    if "[r3-logic] PASS" not in p.stdout:
        raise RuntimeError("logic test failed\n" + p.stdout)
    print("[r3] logic PASS")


def run_lit(backend, out_dir):
    shots = {}
    for on in (1, 0):
        env = os.environ.copy()
        env["DSE_RHI_BACKEND"] = backend
        env["DSE_WUXIA_LIGHTS"] = str(on)
        env["DSE_MAX_FRAMES"] = "12"
        env["DSE_SCREENSHOT_FRAME"] = "8"
        env["DSE_SCREENSHOT_PATH"] = str(out_dir / f"r3_lit_{backend}_{'on' if on else 'off'}.png")
        run([str(BINARY), f"--script={GAME}"], env, timeout=60)
        shots[on] = Path(env["DSE_SCREENSHOT_PATH"])
        if not shots[on].exists():
            raise RuntimeError(f"missing shot {shots[on]}")
    p = subprocess.run([sys.executable, str(STATS), str(shots[1]), str(shots[0])],
                       cwd=ROOT, text=True, encoding="utf-8", errors="replace",
                       capture_output=True, timeout=60)
    if p.returncode != 0:
        raise RuntimeError("stats failed\n" + p.stdout + p.stderr)
    l = parse_luma(p.stdout)
    on_luma = l.get(str(shots[1]), -1.0)
    off_luma = l.get(str(shots[0]), -1.0)
    if on_luma <= off_luma + 2.0:
        raise RuntimeError(f"lit delta too small backend={backend} on={on_luma} off={off_luma}\n{p.stdout}")
    print(f"[r3] lit {backend}: on={on_luma:.2f} off={off_luma:.2f} delta={on_luma-off_luma:.2f}")
    return on_luma, off_luma


def run_game(backend, out_dir, max_frames, shot_frame):
    shot = out_dir / f"r3_game_{backend}.png"
    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_WUXIA_DEMO"] = "1"
    env["DSE_WUXIA_AUTOSAVE"] = "1"
    env["DSE_WUXIA_ACCEPT"] = "1"
    env["DSE_WUXIA_LIGHTS"] = "1"
    env["DSE_MAX_FRAMES"] = str(max_frames)
    env["DSE_SCREENSHOT_FRAME"] = str(shot_frame)
    env["DSE_SCREENSHOT_PATH"] = str(shot)
    p = run([str(BINARY), f"--script={GAME}"], env, timeout=240)
    required = ["[wuxia] map=qingxi_village", "[wuxia] attack", "[wuxia] exp+",
                "[wuxia] gold+", "[wuxia] drop uid=", "[wuxia] autosave ok=true", "DSE_SCREENSHOT_WRITTEN"]
    missing = [s for s in required if s not in p.stdout]
    if missing:
        raise RuntimeError("missing markers: " + ", ".join(missing) + "\n" + p.stdout[-5000:])
    stat = subprocess.run([sys.executable, str(STATS), str(shot)], cwd=ROOT, text=True,
                          encoding="utf-8", errors="replace", capture_output=True, timeout=60)
    if stat.returncode != 0:
        raise RuntimeError("game stats failed\n" + stat.stdout + stat.stderr)
    print(f"[r3] game {backend}: markers ok")
    print(stat.stdout.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backends", default="opengl,vulkan,d3d11")
    ap.add_argument("--out-dir", default="tmp/r3_new_game")
    ap.add_argument("--max-frames", type=int, default=240)
    ap.add_argument("--shot-frame", type=int, default=210)
    args = ap.parse_args()
    if not BINARY.exists():
        print("missing binary", file=sys.stderr); return 2
    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute():
        out_dir = (ROOT / out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    run_logic()
    for backend in [b.strip() for b in args.backends.split(",") if b.strip()]:
        run_lit(backend, out_dir)
        run_game(backend, out_dir, args.max_frames, args.shot_frame)
    print("[r3] new-game acceptance PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())