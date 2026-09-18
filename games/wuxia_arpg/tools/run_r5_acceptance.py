#!/usr/bin/env python3
"""R5 acceptance for games/wuxia_arpg: 3 maps, weather, transitions, R4 regression."""
from __future__ import annotations
import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BINARY = ROOT / "bin" / "dsengine_lua_debug.exe"
STATS = ROOT / "games" / "wuxia_arpg" / "tools" / "pixel_stats.py"
GAME = "games/wuxia_arpg/scripts/main.lua"
MAP_TEST = "games/wuxia_arpg/scripts/_r5_maps_test.lua"
R4_LOGIC = "games/wuxia_arpg/scripts/_r4_logic_test.lua"
R4_COMBAT = "games/wuxia_arpg/scripts/_r4_combat_test.lua"

CASES = [
    ("qingxi_village", "leaf"),
    ("blackwind_stronghold", "storm"),
    ("youhuang_valley", "fog"),
    ("youhuang_valley", "snow"),
]


def run(cmd, env, timeout=120):
    print("[r5] run:", " ".join(cmd))
    p = subprocess.run(cmd, cwd=ROOT, env=env, text=True, encoding="utf-8",
                       errors="replace", capture_output=True, timeout=timeout)
    if p.returncode != 0:
        raise RuntimeError(f"exit={p.returncode}\n{p.stdout}\n{p.stderr}")
    return p


def run_script(script, marker):
    env = os.environ.copy(); env["DSE_WUXIA_ACCEPT"] = "1"
    p = run([str(BINARY), f"--script={script}"], env)
    if marker not in p.stdout:
        raise RuntimeError(f"{script} missing {marker}\n{p.stdout}")
    print(f"[r5] {Path(script).name} PASS")


def run_case(backend, map_id, weather, out_dir):
    shot = out_dir / f"r5_{backend}_{map_id}_{weather}.png"
    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_WUXIA_MAP"] = map_id
    env["DSE_WUXIA_WEATHER"] = weather
    env["DSE_WUXIA_ACCEPT"] = "1"
    env["DSE_MAX_FRAMES"] = "12"
    env["DSE_SCREENSHOT_FRAME"] = "8"
    env["DSE_SCREENSHOT_PATH"] = str(shot)
    p = run([str(BINARY), f"--script={GAME}"], env)
    need = f"[wuxia] map={map_id} weather={weather}"
    if need not in p.stdout or "DSE_SCREENSHOT_WRITTEN" not in p.stdout:
        raise RuntimeError(f"markers missing for {backend}/{map_id}/{weather}\n{p.stdout[-3000:]}")
    if not shot.exists():
        raise RuntimeError(f"missing shot {shot}")
    s = subprocess.run([sys.executable, str(STATS), str(shot)], cwd=ROOT,
                       text=True, encoding="utf-8", errors="replace",
                       capture_output=True, timeout=60)
    if s.returncode != 0:
        raise RuntimeError("stats failed\n" + s.stdout + s.stderr)
    print(f"[r5] case {backend}/{map_id}/{weather} PASS")
    print(s.stdout.strip())


def run_tour(backend, out_dir):
    shot = out_dir / f"r5_tour_{backend}.png"
    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_WUXIA_TOUR"] = "1"
    env["DSE_WUXIA_ACCEPT"] = "1"
    env["DSE_MAX_FRAMES"] = "90"
    env["DSE_SCREENSHOT_FRAME"] = "80"
    env["DSE_SCREENSHOT_PATH"] = str(shot)
    p = run([str(BINARY), f"--script={GAME}"], env, timeout=180)
    for m in ("map=qingxi_village", "map=blackwind_stronghold", "map=youhuang_valley"):
        if m not in p.stdout:
            raise RuntimeError(f"tour missing {m}\n{p.stdout[-4000:]}")
    if not shot.exists():
        raise RuntimeError("missing tour shot")
    print(f"[r5] tour {backend} PASS")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backends", default="opengl,vulkan,d3d11")
    ap.add_argument("--out-dir", default="tmp/r5_wuxia")
    args = ap.parse_args()
    if not BINARY.exists():
        print("missing binary", file=sys.stderr); return 2
    out_dir = Path(args.out_dir)
    if not out_dir.is_absolute(): out_dir = (ROOT / out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    run_script(R4_LOGIC, "[r4-logic] PASS")
    run_script(R4_COMBAT, "[r4-combat] PASS")
    run_script(MAP_TEST, "[r5-maps] PASS")
    for backend in [b.strip() for b in args.backends.split(",") if b.strip()]:
        for map_id, weather in CASES:
            run_case(backend, map_id, weather, out_dir)
        run_tour(backend, out_dir)
    print("[r5] acceptance PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())