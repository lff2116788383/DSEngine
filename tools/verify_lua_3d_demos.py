import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
from typing import Iterable

BASIC_3D_ENTRIES = [
    "3d_triangle",
    "3d_square",
    "3d_cube",
]

P0_3D_ENTRIES = [
    "3d_static_model",
    "3d_material_showcase",
    "3d_lighting_showcase",
    "3d_camera_showcase",
    "3d_textured_cube",
]

P1_3D_ENTRIES = [
    "3d_scene_showcase",
    "3d_skybox_environment",
    "3d_postprocess_showcase",
    "3d_particles_showcase",
    "3d_physics_stack",
]

ENTRY_PRESETS = {
    "basic": BASIC_3D_ENTRIES,
    "p0": P0_3D_ENTRIES,
    "p1": P1_3D_ENTRIES,
    "all": BASIC_3D_ENTRIES + P0_3D_ENTRIES + P1_3D_ENTRIES,
}


def copytree_replace(src: pathlib.Path, dst: pathlib.Path) -> None:
    if dst.exists():
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


def replace_game_entry(config_text: str, entry: str) -> str:
    updated, count = re.subn(r'Config\.game_entry\s*=\s*"[^"]*"', f'Config.game_entry="{entry}"', config_text, count=1)
    if count == 0:
        raise RuntimeError("Config.game_entry not found in config.lua")
    return updated


def run_entry(root: pathlib.Path, exe: pathlib.Path, config_path: pathlib.Path, original_config: str, entry: str, frames: int, timeout: int, out_dir: pathlib.Path) -> int:
    print(f"=== RUN {entry} ===", flush=True)
    config_path.write_text(replace_game_entry(original_config, entry), encoding="utf-8")

    env = os.environ.copy()
    # Let the host use the same bin/samples/lua/main.lua path it uses in normal local runs.
    env.pop("DSE_STARTUP_LUA", None)
    env["DSE_MAX_FRAMES"] = str(frames)
    env["DSE_SCREENSHOT_TARGET"] = "main"
    env["DSE_SCREENSHOT_PATH"] = str(out_dir / f"lua_{entry}.png")
    env["DSE_RENDER_READBACK_DIAG"] = "1"

    log_path = out_dir / f"lua_{entry}.log"
    png_path = out_dir / f"lua_{entry}.png"

    try:
        proc = subprocess.run(
            [str(exe)],
            cwd=str(root),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            timeout=timeout,
        )
        output = proc.stdout or ""
        log_path.write_text(output, encoding="utf-8", errors="replace")
        print(f"=== EXIT {proc.returncode} {entry} ===", flush=True)
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or ""
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        log_path.write_text(output + "\n[TIMEOUT]\n", encoding="utf-8", errors="replace")
        print(f"=== TIMEOUT {entry} ===", flush=True)
        return 124

    tail_lines = []
    for line in output.splitlines():
        if any(token in line for token in ("[3D", "3D-Basic", "Runtime stats", "Readback", "[ERROR]", "[WARN]", "DSE_SCREENSHOT")):
            tail_lines.append(line)
    for line in tail_lines[-30:]:
        print(line, flush=True)

    if not png_path.exists():
        print(f"SCREENSHOT_MISSING {png_path}", flush=True)
        return 2
    print(f"SCREENSHOT_OK {png_path}", flush=True)
    return proc.returncode


def expand_entries(values: Iterable[str]) -> list[str]:
    entries: list[str] = []
    for value in values:
        if value in ENTRY_PRESETS:
            entries.extend(ENTRY_PRESETS[value])
        else:
            entries.append(value)
    deduped: list[str] = []
    for entry in entries:
        if entry not in deduped:
            deduped.append(entry)
    return deduped


def main() -> int:
    parser = argparse.ArgumentParser(description="Verify Lua 3D demos by running the Lua host and writing screenshots/logs.")
    parser.add_argument("--entries", nargs="+", default=["all"], help="Entries or presets: basic, p0, p1, all. Default: all")
    parser.add_argument("--frames", type=int, default=90, help="Frame count before auto-exit. Default: 90")
    parser.add_argument("--timeout", type=int, default=90, help="Timeout seconds per demo. Default: 90")
    parser.add_argument("--out-dir", default="tmp/lua_3d_verify", help="Output directory for screenshots/logs. Default: tmp/lua_3d_verify")
    parser.add_argument("--no-sync", action="store_true", help="Do not copy samples to bin/samples before running")
    args = parser.parse_args()

    root = pathlib.Path.cwd()
    source_samples = root / "samples"
    bin_samples = root / "bin" / "samples"
    config_path = bin_samples / "lua" / "config.lua"
    exe = root / "bin" / "DSEngine_lua_debug.exe"
    out_dir = root / args.out_dir
    out_dir.mkdir(parents=True, exist_ok=True)

    if not exe.exists():
        print(f"ERROR: Lua host not found: {exe}", file=sys.stderr)
        print("Run build_fast_lua.bat first.", file=sys.stderr)
        return 1
    if not source_samples.exists():
        print(f"ERROR: samples directory not found: {source_samples}", file=sys.stderr)
        return 1

    if not args.no_sync:
        print(f"SYNC {source_samples} -> {bin_samples}", flush=True)
        copytree_replace(source_samples, bin_samples)

    if not config_path.exists():
        print(f"ERROR: config not found: {config_path}", file=sys.stderr)
        return 1

    original_config = config_path.read_text(encoding="utf-8-sig")
    entries = expand_entries(args.entries)
    print(f"EXE={exe}", flush=True)
    print(f"CONFIG={config_path}", flush=True)
    print(f"OUT_DIR={out_dir}", flush=True)
    print(f"ENTRIES={','.join(entries)}", flush=True)

    failures: list[tuple[str, int]] = []
    try:
        for entry in entries:
            code = run_entry(root, exe, config_path, original_config, entry, args.frames, args.timeout, out_dir)
            if code != 0:
                failures.append((entry, code))
    finally:
        config_path.write_text(original_config, encoding="utf-8")

    if failures:
        print("FAILED_ENTRIES=" + ",".join(f"{entry}:{code}" for entry, code in failures), flush=True)
        return 1
    print("VERIFY_OK", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
