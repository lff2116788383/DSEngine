#!/usr/bin/env python3
"""R4 acceptance for games/wuxia_arpg: combat, growth, gear, boss, three backends."""
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
LOGIC = "games/wuxia_arpg/scripts/_r4_logic_test.lua"
COMBAT = "games/wuxia_arpg/scripts/_r4_combat_test.lua"
GAME = "games/wuxia_arpg/scripts/main.lua"


def run(cmd, env, timeout=300):
    print("[r4] run:", " ".join(cmd))
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


def run_script(script: str, marker: str, env_extra=None, timeout=90):
    env = os.environ.copy()
    env["DSE_WUXIA_ACCEPT"] = "1"
    if env_extra:
        env.update(env_extra)
    p = run([str(BINARY), f"--script={script}"], env, timeout=timeout)
    if marker not in p.stdout:
        raise RuntimeError(f"{script} missing {marker}\n{p.stdout}")
    print(f"[r4] {Path(script).name} PASS")


def run_lit(backend, out_dir):
    shots = {}
    for on in (1, 0):
        env = os.environ.copy()
        env["DSE_RHI_BACKEND"] = backend
        env["DSE_WUXIA_LIGHTS"] = str(on)
        env["DSE_MAX_FRAMES"] = "12"
        env["DSE_SCREENSHOT_FRAME"] = "8"
        env["DSE_SCREENSHOT_PATH"] = str(out_dir / f"r4_lit_{backend}_{'on' if on else 'off'}.png")
        run([str(BINARY), f"--script={GAME}"], env, timeout=90)
        shots[on] = Path(env["DSE_SCREENSHOT_PATH"])
        if not shots[on].exists():
            raise RuntimeError(f"missing lit shot {shots[on]}")
    p = subprocess.run([sys.executable, str(STATS), str(shots[1]), str(shots[0])],
                       cwd=ROOT, text=True, encoding="utf-8", errors="replace",
                       capture_output=True, timeout=60)
    l = parse_luma(p.stdout)
    on_luma = l.get(str(shots[1]), -1.0); off_luma = l.get(str(shots[0]), -1.0)
    if on_luma <= off_luma + 2.0:
        raise RuntimeError(f"lit delta too small {backend}: {on_luma} vs {off_luma}\n{p.stdout}")
    print(f"[r4] lit {backend}: on={on_luma:.2f} off={off_luma:.2f} delta={on_luma-off_luma:.2f}")


def run_game(backend, out_dir, max_frames, shot_frame):
    shot = out_dir / f"r4_game_{backend}.png"
    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_WUXIA_DEMO"] = "1"
    env["DSE_WUXIA_AUTOSAVE"] = "1"
    env["DSE_WUXIA_ACCEPT"] = "1"
    env["DSE_WUXIA_LIGHTS"] = "1"
    env["DSE_MAX_FRAMES"] = str(max_frames)
    env["DSE_SCREENSHOT_FRAME"] = str(shot_frame)
    env["DSE_SCREENSHOT_PATH"] = str(shot)
    p = run([str(BINARY), f"--script={GAME}"], env, timeout=300)
    required = [
        "[wuxia] combo=2", "[wuxia] combo=3", "[wuxia] skill=fenhua",
        "[wuxia] boss_phase=2", "[wuxia] boss_phase=3", "[wuxia] boss_dead",
        "[wuxia] equip slot=", "[wuxia] autosave ok=true", "DSE_SCREENSHOT_WRITTEN",
    ]
    missing = [s for s in required if s not in p.stdout]
    if missing:
        raise RuntimeError("demo missing markers: " + ", ".join(missing) + "\n" + p.stdout[-6000:])
    if not shot.exists():
        raise RuntimeError(f"missing game shot {shot}")
    stat = subprocess.run([sys.executable, str(STATS), str(shot)], cwd=ROOT,
                          text=True, encoding="utf-8", errors="replace",
                          capture_output=True, timeout=60)
    print(f"[r4] game {backend}: markers ok")
    print(stat.stdout.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backends", default="opengl")
    ap.add_argument("--out-dir", default="tmp/r4_wuxia")
    ap.add_argument("--max-frames", type=int, default=600)
    ap.add_argument("--shot-frame", type=int, default=560)
    args = ap.parse_args()
    if not BINARY.exists():
        print("missing binary", file=sys.stderr); return 2
    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute(): out_dir = (ROOT / out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    run_script(LOGIC, "[r4-logic] PASS")
    run_script(COMBAT, "[r4-combat] PASS")
    for backend in [b.strip() for b in args.backends.split(",") if b.strip()]:
        run_lit(backend, out_dir)
        run_game(backend, out_dir, args.max_frames, args.shot_frame)
    print("[r4] acceptance PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())