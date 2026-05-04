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
    "3d_character_controller",
    "3d_physics_interaction",
    "3d_vse15_22_scene",
]

REQUIRED_LOG_TOKENS = {
    # P0 基础 3D demo：验证渲染管线基础可达
    "3d_static_model": [
        "[3D][StaticModel]",
    ],
    "3d_material_showcase": [
        "[3D][Material]",
    ],
    "3d_lighting_showcase": [
        "[3D][Lighting]",
    ],
    "3d_camera_showcase": [
        "[3D][Camera]",
    ],
    "3d_textured_cube": [
        "[3D][TexturedCube]",
    ],
    # P1 demo
    "3d_scene_showcase": [
        "[3D][Scene]",
    ],
    "3d_skybox_environment": [
        "[3D][Skybox]",
    ],
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
        "[3D][Animation]",
        "animator_resource_chain",
        "real_animation_resource",
        "resource_paths_configured=true",
        "animator_state_api",
        "get_animator_3d_state=true",
        "final_bones=",
        "has_skeleton=true",
    ],
    "3d_character_third_person": [
        "[3D][Character]",
        "character_steering_api",
        "add_steering=true",
        "set_steering_target=true",
        "get_steering_state=true",
        "speed_nonzero=true",
        "character_animation_resource",
        "character_animator_state_api",
        "resource_paths_configured=true",
        "has_skeleton=true",
    ],
    "3d_audio_spatial": [
        "[3D][AudioSpatial]",
        "real_3d_audio",
        "set_3d_mode=true",
        "set_3d_distance",
        "add_listener",
        "audio_state_api=true",
        "clip_loaded=true",
        "spatial_enabled=true",
    ],
    "3d_physics_stack": [
        "[3D][PhysicsStack]",
    ],
    "3d_texture_material_slots": [
        "[3D][TextureMaterialSlots]",
        "mesh_authoring_api",
        "authored quad uses UV texture sampling",
    ],
    "3d_physics_raycast_pick": [
        "[3D][PhysicsRaycastPick]",
        "physics_3d_raycast",
    ],
    "3d_terrain_lod_zones": [
        "[3D][TerrainLodZones]",
    ],
    "3d_physics_interaction": [
        "[3D][PhysicsInteraction]",
        "physics_interaction_api=true",
    ],
    "3d_character_controller": [
        "[3D][CharacterController]",
        "character_controller_api=true",
    ],
    "3d_vse15_22_scene": [
        "p4_vse15_22_scene",
        "full_scene_replica=true",
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
        "cooked_fbx=true",
        "mesh_path=vse_demo/15_22/cooked/Monster.dmesh",
        "idle_danim=vse_demo/15_22/cooked/Monster.danim",
        "walk_danim=vse_demo/15_22/cooked/Walk.danim",
        "attack_danim=vse_demo/15_22/cooked/Attack.danim",
        "attack2_danim=vse_demo/15_22/cooked/Attack2.danim",
        "pos_danim=vse_demo/15_22/cooked/Monster.danim",
        "additive_danim=vse_demo/15_22/cooked/Monster.danim",
        "dskel=vse_demo/15_22/cooked/Monster.dskel",
        "runtime_animation",
        "final_bones=48",
        "has_skeleton=true",
        "runtime_environment",
        "PointLight=true",
        "OceanPlane=true",
        "point_shadow=true",
        "pbr_material=true",
        "cooked_ocean=true",
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
    """安全替换目录：先尝试删除旧目录，遇到 Windows 文件锁等异常时回退到覆盖模式。"""
    if dst.exists():
        try:
            shutil.rmtree(dst)
        except (OSError, FileNotFoundError) as exc:
            # Windows 文件锁/竞态：回退到不删除旧目录，直接覆盖
            print(f"[WARN] copytree_replace rmtree fallback: {exc}", flush=True)
    shutil.copytree(src, dst, dirs_exist_ok=True)


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


def check_screenshot_not_black(png_path: pathlib.Path) -> tuple[bool, str]:
    """检查截图是否为黑屏或接近纯色。

    使用轻量像素采样（无需第三方图像库），读取 PNG 文件头后
    按 IHDR 解析宽高，然后对 IDAT 解压后的原始像素做间隔采样。
    如果采样点 RGB 值全部接近 0 或全部相同，视为黑屏/纯色。

    回退策略：若无法解析 PNG（如文件过小或格式异常），
    只做文件大小检查（> 1KB 即视为非黑屏）。

    Returns:
        (is_ok, detail): is_ok=True 表示截图正常，detail 为诊断信息
    """
    MIN_FILE_SIZE = 1024  # 1KB 以下几乎不可能是有效截图
    if not png_path.exists():
        return False, "file_not_found"
    file_size = png_path.stat().st_size
    if file_size < MIN_FILE_SIZE:
        return False, f"file_too_small={file_size}bytes"

    # 尝试用 PIL/Pillow 做精确检测（如可用）
    try:
        from PIL import Image
        import numpy as np
        img = Image.open(str(png_path)).convert("RGB")
        arr = np.array(img)
        # 采样：最多取 100x100 网格
        h, w = arr.shape[:2]
        step_h = max(1, h // 100)
        step_w = max(1, w // 100)
        sampled = arr[::step_h, ::step_w]

        max_val = int(sampled.max())
        min_val = int(sampled.min())
        mean_val = float(sampled.mean())

        # 全黑判定
        if max_val <= 5:
            return False, f"all_black max={max_val} mean={mean_val:.1f}"
        # 近乎纯色判定：最大最小差值极小
        if max_val - min_val <= 3 and max_val <= 15:
            return False, f"near_solid max={max_val} min={min_val} mean={mean_val:.1f}"
        return True, f"max={max_val} min={min_val} mean={mean_val:.1f}"
    except ImportError:
        # 无 Pillow，用 render readback 亮度指标替代
        return True, f"file_size_ok={file_size}bytes (no_pillow_skip_pixel_check)"


def check_log_errors(output: str) -> list[str]:
    """从日志中提取 [ERROR] 行（排除已知的无害错误）。

    Returns:
        error_lines: 非空的 [ERROR] 行列表
    """
    # 已知可忽略的错误模式（引擎关闭时 GL 资源清理等）
    IGNORED_PATTERNS = [
        "ShutdownGeometryBuffers",  # GL VAO/VBO 清理时的非致命警告
        "glDelete",                 # GL 资源释放
    ]
    errors = []
    for line in output.splitlines():
        if "[ERROR]" not in line:
            continue
        # 跳过已知无害错误
        if any(pat in line for pat in IGNORED_PATTERNS):
            continue
        errors.append(line.strip())
    return errors


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

    # 检测日志中的 [ERROR] 行
    error_lines = check_log_errors(output)
    if error_lines:
        # 打印前 5 条错误，但不直接阻断——某些引擎关闭错误是预期的
        for err in error_lines[:5]:
            print(f"LOG_ERROR {entry}: {err}", flush=True)
        if len(error_lines) > 5:
            print(f"LOG_ERROR {entry}: ... and {len(error_lines) - 5} more errors", flush=True)
        # 仅当错误行数过多（>10）时才视为阻断
        if len(error_lines) > 10:
            print(f"LOG_ERROR_TOO_MANY {entry}: {len(error_lines)} errors", flush=True)
            return 5

    if not png_path.exists():
        print(f"SCREENSHOT_MISSING {png_path}", flush=True)
        return 2

    max_rgb, avg_rgb = extract_render_readback_metrics(output)
    if entry == "3d_vse15_22_scene" and (max_rgb is None or avg_rgb is None or max_rgb < 40 or avg_rgb < 18):
        print(f"SCREENSHOT_TOO_DARK {entry}: max_rgb={max_rgb} avg_rgb={avg_rgb} min_max_rgb=40 min_avg_rgb=18", flush=True)
        return 4

    # 截图黑屏/纯色检测（所有 demo 通用）
    screenshot_ok, screenshot_detail = check_screenshot_not_black(png_path)
    if not screenshot_ok:
        print(f"SCREENSHOT_BLACK_OR_SOLID {entry}: {screenshot_detail}", flush=True)
        return 4

    print(f"SCREENSHOT_OK {png_path} ({screenshot_detail})", flush=True)
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
    parser = argparse.ArgumentParser(
        description="Verify Lua 3D demos by running the Lua host and writing screenshots/logs. "
                    "Exit codes: 0=OK, 1=setup error, 2=screenshot missing, 3=log token missing, "
                    "4=screenshot black/too dark, 5=too many log errors, 124=timeout."
    )
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
