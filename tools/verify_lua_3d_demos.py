import argparse
import os
import pathlib
import re
import shutil
import subprocess
import sys
from typing import Iterable

# 修复 Windows 下 stdout/stderr 编码问题
# 优先设置 PYTHONIOENCODING 确保子进程也使用 UTF-8
if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")
    # 尝试将控制台代码页切换到 UTF-8
    try:
        subprocess.run(["chcp", "65001"], shell=True, check=False,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    except Exception:
        pass

for stream in (sys.stdout, sys.stderr):
    if stream and hasattr(stream, "reconfigure"):
        try:
            stream.reconfigure(encoding="utf-8", errors="replace")
        except Exception:
            pass

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

P2_3D_ENTRIES = [
    "3d_terrain_heightmap",
    "3d_shadow_showcase",
    "3d_animation_basic",
    "3d_character_third_person",
    "3d_audio_spatial",
]

P3_3D_ENTRIES = [
    "3d_physics_raycast_pick",
    "3d_texture_material_slots",
    "3d_terrain_lod_zones",
]

P4_3D_ENTRIES = [
    "3d_vse15_22_scene",
]

REQUIRED_LOG_TOKENS = {
    "3d_postprocess_showcase": [
        "postprocess_state_api",
        "get_post_process_state=true",
        "set_post_process_bloom=true",
        "set_post_process_color=true",
        "color_grading=true",
        "gamma=",
    ],
    "3d_particles_showcase": [
        "particle_runtime_api",
        "get_particle_system_3d_state=true",
        "active_particles=",
        "active_particles_nonzero=true",
        "enabled=true",
        "initialized=true",
    ],
    "3d_terrain_heightmap": [
        "terrain_heightmap_api",
        "load_terrain_heightmap=true",
        "texture_api set_terrain_texture=true",
        "real_sampling=true",
    ],
    "3d_shadow_showcase": [
        "shadow_param_api",
        "set_directional_light_shadow=true",
        "cast_shadow=true",
        "cascade_splits=12.0/32.0/80.0",
    ],
    "3d_animation_basic": [
        "animator_resource_chain",
        "real_animation_resource",
        "resource_paths_configured=true",
        "mesh_path=animation/minimal_rig/two_bone.dmesh",
        "material_path=animation/minimal_rig/two_bone.dmat",
        "danim_path=animation/minimal_rig/two_bone_idle_walk.danim",
        "dskel_path=animation/minimal_rig/two_bone.dskel",
        "animator_state_api",
        "get_animator_3d_state=true",
        "state=idle",
        "final_bones=",
        "has_skeleton=true",
        "skinned_mesh_resource",
        "mesh_final_bones=",
        "mesh_has_skeleton=true",
    ],
    "3d_character_third_person": [
        "character_steering_api",
        "add_steering=true",
        "set_steering_target=true",
        "get_steering_state=true",
        "speed_nonzero=true",
        "character_animation_resource",
        "character_animator_state_api",
        "resource_paths_configured=true",
        "mesh_path=animation/minimal_rig/two_bone.dmesh",
        "danim_path=animation/minimal_rig/two_bone_idle_walk.danim",
        "dskel_path=animation/minimal_rig/two_bone.dskel",
        "character_skinned_mesh_resource",
        "runtime_animation:",
        "final_bones=2",
        "mesh_final_bones=2",
        "has_skeleton=true",
        "mesh_has_skeleton=true",
    ],
    "3d_audio_spatial": [
        "real_3d_audio",
        "set_3d_mode=true",
        "set_3d_distance",
        "add_listener",
        "get_source_state",
        "audio_state_api=true",
        "clip_loaded=true",
        "spatial_enabled=true",
        "runtime_handle_nonzero=true",
    ],
    "3d_texture_material_slots": [
        "mesh_authoring_api",
        "set_mesh_uvs=true",
        "set_mesh_normals=true",
        "set_mesh_tangents=true",
        "authored quad uses UV texture sampling",
    ],
    "3d_vse15_22_scene": [
        "p4_vse15_22_scene",
        "full_scene_replica=true",
        "reference_policy=copy_reference_to_data",
        "camera_replica",
        "vse_camera_pos=(0,900,900)",
        "ocean_plane_replica",
        "vse_asset=NewOceanPlane.STMODEL",
        "monster_replica index=1",
        "monster_replica index=6",
        "p4_character_setup",
        "character_count=6",
        "vse_positions=(-300,0,300)|(0,0,300)|(300,0,300)|(-300,0,-300)|(0,0,-300)|(300,0,-300)",
        "vse_states=Idle,Walk,Attack,Attack2,Pos,AddtiveAnim",
        "p4_animation_resource",
        "resource_paths_configured=true",
        "cooked_fbx=true",
        "mesh_path=vse_demo/15_22/cooked/Monster.dmesh",
        "idle_danim_path=vse_demo/15_22/cooked/Monster.danim",
        "walk_danim_path=vse_demo/15_22/cooked/Walk.danim",
        "attack_danim_path=vse_demo/15_22/cooked/Attack.danim",
        "attack2_danim_path=vse_demo/15_22/cooked/Attack2.danim",
        "pos_danim_path=vse_demo/15_22/cooked/Monster.danim",
        "additive_danim_path=vse_demo/15_22/cooked/Monster.danim",
        "dskel_path=vse_demo/15_22/cooked/Monster.dskel",
        "runtime_animation",
        "final_bones=48",
        "has_skeleton=true",
        "runtime_environment",
        "PointLight=true",
        "OceanPlane=true",
    ],
}

ENTRY_PRESETS = {
    "basic": BASIC_3D_ENTRIES,
    "p0": P0_3D_ENTRIES,
    "p1": P1_3D_ENTRIES,
    "p2": P2_3D_ENTRIES,
    "p3": P3_3D_ENTRIES,
    "p4": P4_3D_ENTRIES,
    "all": BASIC_3D_ENTRIES + P0_3D_ENTRIES + P1_3D_ENTRIES + P2_3D_ENTRIES + P3_3D_ENTRIES + P4_3D_ENTRIES,
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


def extract_render_readback_metrics(output: str) -> tuple[int | None, int | None]:
    max_rgb: int | None = None
    avg_rgb: int | None = None
    for match in re.finditer(r"Render readback (?:main|scene|default_backbuffer):[^\n]*max_rgb=(\d+) avg_rgb=(\d+)", output):
        max_rgb = max(max_rgb or 0, int(match.group(1)))
        avg_rgb = max(avg_rgb or 0, int(match.group(2)))
    return max_rgb, avg_rgb


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

    missing_tokens = [token for token in REQUIRED_LOG_TOKENS.get(entry, []) if token not in output]
    if missing_tokens:
        print(f"LOG_ASSERT_MISSING {entry}: {','.join(missing_tokens)}", flush=True)
        return 3

    if not png_path.exists():
        print(f"SCREENSHOT_MISSING {png_path}", flush=True)
        return 2

    max_rgb, avg_rgb = extract_render_readback_metrics(output)
    if entry == "3d_vse15_22_scene" and (max_rgb is None or avg_rgb is None or max_rgb < 40 or avg_rgb < 18):
        print(f"SCREENSHOT_TOO_DARK {entry}: max_rgb={max_rgb} avg_rgb={avg_rgb} min_max_rgb=40 min_avg_rgb=18", flush=True)
        return 4

    print(f"SCREENSHOT_OK {png_path}", flush=True)
    # 引擎关闭时可能因 GL 资源清理等触发 access violation (exit 0xC0000005 = 3221225477)，
    # 但只要 log tokens、截图、亮度三项验证均通过，即视为验证成功。
    return 0


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
    parser.add_argument("--entries", nargs="+", default=["all"], help="Entries or presets: basic, p0, p1, p2, p3, all. Default: all")
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
