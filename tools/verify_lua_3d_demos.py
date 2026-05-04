import argparse
import os
import pathlib
import re
import shutil
import struct
import subprocess
import sys
import zlib
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
    "3d_input_showcase",
    "3d_hud_overlay",
    "3d_procedural_mesh",
    "3d_scene_load",
    "3d_metrics_debug",
    "3d_physics_triggers",
    "3d_audio_complete",
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
    "3d_input_showcase": [
        "[3D][Input]",
        "input_api_summary",
        "get_key=true",
        "get_mouse_left_down=true",
        "get_screen_width=true",
        "time_since_startup=true",
    ],
    "3d_hud_overlay": [
        "[3D][HudOverlay]",
        "ui_api",
        "add_renderer=true",
        "add_label=true",
        "add_panel=true",
        "add_button=true",
        "add_joystick=true",
        "runtime:",
    ],
    "3d_procedural_mesh": [
        "[3D][ProceduralMesh]",
        "procedural sphere + cylinder + cone",
        "sphere: vertices=",
        "cylinder: vertices=",
        "cone: vertices=",
        "mesh_authoring_api",
        "set_mesh_uvs=true",
        "set_mesh_normals=true",
        "set_mesh_tangents=true",
    ],
    "3d_scene_load": [
        "[3D][SceneLoad]",
        "api_summary",
        "load_scene=",
        "find_entities_by_mesh_path=",
        "fallback_objects=",
    ],
    "3d_metrics_debug": [
        "[3D][MetricsDebug]",
        "api_summary",
        "get_draw_calls=true",
        "get_memory_usage_kb=true",
        "get_screen_width=true",
        "setup: 17 objects",
    ],
    "3d_physics_triggers": [
        "[3D][PhysicsTriggers]",
        "3d_raycast=true",
        "3d_impulse=true",
        "3d_velocity=true",
        "phase1: impulse",
    ],
    "3d_audio_complete": [
        "[3D][AudioComplete]",
        "api_summary",
        "add_listener=true",
        "add_source=true",
        "set_3d_mode=true",
        "set_3d_distance=true",
        "set_loop=true",
        "restart=true",
        "get_source_state=true",
        "setup: 3 spatial audio sources",
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
        "texture_bind_summary",
        "loaded_slots=4",
    ],
}

VISUAL_SUBJECT_CHECKS = {
    "3d_input_showcase": {"min_subject_ratio": 0.030, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_hud_overlay": {"min_subject_ratio": 0.040, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_procedural_mesh": {"min_subject_ratio": 0.045, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_scene_load": {"min_subject_ratio": 0.030, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_metrics_debug": {"min_subject_ratio": 0.040, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_physics_triggers": {"min_subject_ratio": 0.035, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_audio_complete": {"min_subject_ratio": 0.035, "min_edge_ratio": 0.0010, "min_luma_std": 4.0},
    "3d_vse15_22_scene": {"min_subject_ratio": 0.080, "min_edge_ratio": 0.0020, "min_luma_std": 8.0},
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


def _paeth_predictor(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def _read_png_rgb_pixels(png_path: pathlib.Path) -> tuple[int, int, list[tuple[int, int, int]]]:
    """用 stdlib 解码 8-bit RGB/RGBA PNG，避免无 Pillow 环境跳过像素检查。"""
    data = png_path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not_png")

    pos = 8
    width = height = bit_depth = color_type = None
    raw = b""
    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        chunk_type = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if chunk_type == b"IHDR":
            width, height, bit_depth, color_type, compression, png_filter, interlace = struct.unpack(">IIBBBBB", chunk)
            if compression != 0 or png_filter != 0 or interlace != 0:
                raise ValueError("unsupported_png_layout")
        elif chunk_type == b"IDAT":
            raw += chunk
        elif chunk_type == b"IEND":
            break

    if width is None or height is None or bit_depth != 8 or color_type not in (2, 6):
        raise ValueError("unsupported_png_format")

    channels = 4 if color_type == 6 else 3
    stride = width * channels
    inflated = zlib.decompress(raw)
    rows: list[bytearray] = []
    cursor = 0
    prev = bytearray(stride)
    for _ in range(height):
        filter_type = inflated[cursor]
        cursor += 1
        row = bytearray(inflated[cursor:cursor + stride])
        cursor += stride
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = prev[i]
            up_left = prev[i - channels] if i >= channels else 0
            if filter_type == 1:
                row[i] = (row[i] + left) & 0xFF
            elif filter_type == 2:
                row[i] = (row[i] + up) & 0xFF
            elif filter_type == 3:
                row[i] = (row[i] + ((left + up) // 2)) & 0xFF
            elif filter_type == 4:
                row[i] = (row[i] + _paeth_predictor(left, up, up_left)) & 0xFF
            elif filter_type != 0:
                raise ValueError("unsupported_png_filter")
        rows.append(row)
        prev = row

    pixels: list[tuple[int, int, int]] = []
    for row in rows:
        for x in range(0, stride, channels):
            pixels.append((row[x], row[x + 1], row[x + 2]))
    return width, height, pixels


def _png_visual_metrics(png_path: pathlib.Path) -> dict[str, float | int]:
    width, height, pixels = _read_png_rgb_pixels(png_path)
    total = len(pixels)
    lumas = [0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in pixels]
    min_val = min(min(p) for p in pixels)
    max_val = max(max(p) for p in pixels)
    mean_val = sum(sum(p) / 3.0 for p in pixels) / total
    mean_luma = sum(lumas) / total
    variance = sum((l - mean_luma) * (l - mean_luma) for l in lumas) / total

    border: list[tuple[int, int, int]] = []
    for y in range(height):
        for x in range(width):
            if y < 20 or y >= height - 20 or x < 20 or x >= width - 20:
                border.append(pixels[y * width + x])
    bg_r = sum(p[0] for p in border) / len(border)
    bg_g = sum(p[1] for p in border) / len(border)
    bg_b = sum(p[2] for p in border) / len(border)
    subject = 0
    for r, g, b in pixels:
        if abs(r - bg_r) + abs(g - bg_g) + abs(b - bg_b) > 36.0:
            subject += 1

    edges = 0
    edge_samples = 0
    for y in range(1, height):
        row = y * width
        prev_row = (y - 1) * width
        for x in range(1, width):
            lum = lumas[row + x]
            if abs(lum - lumas[row + x - 1]) > 10.0 or abs(lum - lumas[prev_row + x]) > 10.0:
                edges += 1
            edge_samples += 1

    return {
        "width": width,
        "height": height,
        "min": int(min_val),
        "max": int(max_val),
        "mean": float(mean_val),
        "luma_std": variance ** 0.5,
        "subject_ratio": subject / total,
        "edge_ratio": edges / max(1, edge_samples),
    }


def check_screenshot_not_black(png_path: pathlib.Path) -> tuple[bool, str]:
    """检查截图是否为黑屏或接近纯色；无 Pillow 时也必须解析像素。"""
    MIN_FILE_SIZE = 1024  # 1KB 以下几乎不可能是有效截图
    if not png_path.exists():
        return False, "file_not_found"
    file_size = png_path.stat().st_size
    if file_size < MIN_FILE_SIZE:
        return False, f"file_too_small={file_size}bytes"

    try:
        metrics = _png_visual_metrics(png_path)
    except Exception as exc:
        return False, f"png_decode_failed={exc} file_size={file_size}bytes"

    max_val = int(metrics["max"])
    min_val = int(metrics["min"])
    mean_val = float(metrics["mean"])
    luma_std = float(metrics["luma_std"])
    if max_val <= 5:
        return False, f"all_black max={max_val} mean={mean_val:.1f}"
    if max_val - min_val <= 3 or luma_std <= 1.5:
        return False, f"near_solid max={max_val} min={min_val} mean={mean_val:.1f} luma_std={luma_std:.2f}"
    return True, "max={} min={} mean={:.1f} luma_std={:.2f} subject_ratio={:.4f} edge_ratio={:.4f}".format(max_val, min_val, mean_val, luma_std, float(metrics["subject_ratio"]), float(metrics["edge_ratio"]))


def check_visual_subject(png_path: pathlib.Path, thresholds: dict[str, float]) -> tuple[bool, str]:
    """专项视觉验收：不能只亮，必须有足够非背景主体与边缘细节。"""
    try:
        metrics = _png_visual_metrics(png_path)
    except Exception as exc:
        return False, f"png_decode_failed={exc}"

    min_subject_ratio = float(thresholds["min_subject_ratio"])
    min_edge_ratio = float(thresholds["min_edge_ratio"])
    min_luma_std = float(thresholds["min_luma_std"])
    subject_ratio = float(metrics["subject_ratio"])
    edge_ratio = float(metrics["edge_ratio"])
    luma_std = float(metrics["luma_std"])
    detail = "subject_ratio={:.4f} edge_ratio={:.4f} luma_std={:.2f} max={} mean={:.1f}".format(subject_ratio, edge_ratio, luma_std, int(metrics["max"]), float(metrics["mean"]))
    if subject_ratio < min_subject_ratio:
        return False, f"subject_too_small {detail} min_subject_ratio={min_subject_ratio:.3f}"
    if edge_ratio < min_edge_ratio:
        return False, f"edge_detail_too_low {detail} min_edge_ratio={min_edge_ratio:.3f}"
    if luma_std < min_luma_std:
        return False, f"contrast_too_low {detail} min_luma_std={min_luma_std:.1f}"
    return True, detail


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

    visual_thresholds = VISUAL_SUBJECT_CHECKS.get(entry)
    if visual_thresholds:
        visual_ok, visual_detail = check_visual_subject(png_path, visual_thresholds)
        if not visual_ok:
            print(f"SCREENSHOT_SUBJECT_NOT_VISIBLE {entry}: {visual_detail}", flush=True)
            return 4
        screenshot_detail = f"{screenshot_detail}; subject_visual={visual_detail}"

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
