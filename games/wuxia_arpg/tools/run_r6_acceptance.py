#!/usr/bin/env python3
"""R6 final acceptance: save/load, R4/R5 regressions, full 3x3 matrix, asset audit."""
from __future__ import annotations
import argparse
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BINARY = ROOT / "bin" / "dsengine_lua_debug.exe"
GAME_DIR = ROOT / "games" / "wuxia_arpg"
LEDGER = ROOT / "docs" / "design" / "WUXIA_ARPG_NEW_ASSET_LEDGER.csv"
R5_RUNNER = ROOT / "games" / "wuxia_arpg" / "tools" / "run_r5_acceptance.py"


def run(cmd, env=None, timeout=300):
    print("[r6] run:", " ".join(cmd))
    p = subprocess.run(cmd, cwd=ROOT, env=env, text=True, encoding="utf-8",
                       errors="replace", capture_output=True, timeout=timeout)
    if p.returncode != 0:
        raise RuntimeError(f"exit={p.returncode}\n{p.stdout}\n{p.stderr}")
    return p


def run_script(script, marker):
    env = os.environ.copy(); env["DSE_WUXIA_ACCEPT"] = "1"
    p = run([str(BINARY), f"--script={script}"], env, timeout=90)
    if marker not in p.stdout:
        raise RuntimeError(f"{script} missing {marker}\n{p.stdout}")
    print(f"[r6] {Path(script).name} PASS")


def audit_assets():
    bad = ("SimHei", "逸剑风云决", "commercial game")
    hits = []
    for p in GAME_DIR.rglob("*"):
        if not p.is_file() or p.suffix.lower() not in (".lua", ".py", ".md", ".json"):
            continue
        if p.name == "run_r6_acceptance.py":
            continue
        try:
            text = p.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        if p.name == "run_r6_acceptance.py":
            continue
        for token in bad:
            if token in text:
                hits.append(f"{p.relative_to(ROOT)}: {token}")
    if hits:
        raise RuntimeError("asset audit hits:\n" + "\n".join(hits))
    if not LEDGER.exists():
        raise RuntimeError("missing new asset ledger")
    rows = len(LEDGER.read_text(encoding="utf-8").splitlines()) - 1
    if rows < 60:
        raise RuntimeError(f"asset ledger rows too small: {rows}")
    print(f"[r6] asset audit PASS rows={rows}")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--backends", default="opengl,vulkan,d3d11")
    ap.add_argument("--out-dir", default="tmp/r6_final")
    args = ap.parse_args()
    if not BINARY.exists():
        print("missing binary", file=sys.stderr); return 2
    run_script("games/wuxia_arpg/scripts/_r6_save_test.lua", "[r6-save] PASS")
    run_script("games/wuxia_arpg/scripts/_r4_logic_test.lua", "[r4-logic] PASS")
    run_script("games/wuxia_arpg/scripts/_r4_combat_test.lua", "[r4-combat] PASS")
    run_script("games/wuxia_arpg/scripts/_r5_maps_test.lua", "[r5-maps] PASS")
    audit_assets()
    p = run([sys.executable, str(R5_RUNNER), "--backends", args.backends, "--out-dir", args.out_dir], timeout=300)
    if "[r5] acceptance PASS" not in p.stdout:
        raise RuntimeError("R5 full matrix did not PASS\n" + p.stdout[-6000:])
    print(p.stdout[-4000:])
    print("[r6] acceptance PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())