#!/usr/bin/env python3
"""
verify_scene.py — 自动运行 KF_Framework demo 并截图分析

用法:
    python tools/verify_scene.py                    # 默认 120 帧后截图
    python tools/verify_scene.py --frames 60        # 60 帧
    python tools/verify_scene.py --open              # 截图后自动打开
"""

import argparse
import math
import os
import pathlib
import struct
import subprocess
import sys
import zlib

if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")

# ============================================================
# PNG 纯 stdlib 解码（来自 verify_lua_3d_demos.py）
# ============================================================

def _paeth_predictor(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    return b if pb <= pc else c


def read_png_rgb(png_path: pathlib.Path):
    """解码 8-bit RGB/RGBA PNG，返回 (width, height, [(r,g,b)...])"""
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
            width, height, bit_depth, color_type = struct.unpack(">IIBB", chunk[:10])
        elif chunk_type == b"IDAT":
            raw += chunk
        elif chunk_type == b"IEND":
            break
    if width is None or bit_depth != 8 or color_type not in (2, 6):
        raise ValueError(f"unsupported format: {bit_depth}bit color_type={color_type}")
    channels = 4 if color_type == 6 else 3
    stride = width * channels
    inflated = zlib.decompress(raw)
    rows = []
    cursor = 0
    prev = bytearray(stride)
    for _ in range(height):
        ft = inflated[cursor]; cursor += 1
        row = bytearray(inflated[cursor:cursor + stride]); cursor += stride
        for i in range(stride):
            left = row[i - channels] if i >= channels else 0
            up = prev[i]
            up_left = prev[i - channels] if i >= channels else 0
            if ft == 1:   row[i] = (row[i] + left) & 0xFF
            elif ft == 2: row[i] = (row[i] + up) & 0xFF
            elif ft == 3: row[i] = (row[i] + ((left + up) // 2)) & 0xFF
            elif ft == 4: row[i] = (row[i] + _paeth_predictor(left, up, up_left)) & 0xFF
        rows.append(row); prev = row
    pixels = []
    for row in rows:
        for x in range(0, stride, channels):
            pixels.append((row[x], row[x + 1], row[x + 2]))
    return width, height, pixels


# ============================================================
# 截图分析
# ============================================================

def analyze_screenshot(png_path: pathlib.Path) -> dict:
    width, height, pixels = read_png_rgb(png_path)
    total = len(pixels)
    lumas = [0.2126 * r + 0.7152 * g + 0.0722 * b for r, g, b in pixels]
    min_val = min(min(p) for p in pixels)
    max_val = max(max(p) for p in pixels)
    mean_luma = sum(lumas) / total
    variance = sum((l - mean_luma) ** 2 for l in lumas) / total
    luma_std = math.sqrt(variance)

    # 背景色估算（边缘 20px）
    border = []
    for y in range(height):
        for x in range(width):
            if y < 20 or y >= height - 20 or x < 20 or x >= width - 20:
                border.append(pixels[y * width + x])
    bg_r = sum(p[0] for p in border) / len(border)
    bg_g = sum(p[1] for p in border) / len(border)
    bg_b = sum(p[2] for p in border) / len(border)
    subject = sum(1 for r, g, b in pixels if abs(r - bg_r) + abs(g - bg_g) + abs(b - bg_b) > 36.0)

    # 边缘检测
    edges = edge_samples = 0
    for y in range(1, height):
        row_off = y * width
        prev_off = (y - 1) * width
        for x in range(1, width):
            lum = lumas[row_off + x]
            if abs(lum - lumas[row_off + x - 1]) > 10.0 or abs(lum - lumas[prev_off + x]) > 10.0:
                edges += 1
            edge_samples += 1

    # 颜色分布（粗略色相直方图）
    warm = cool = neutral = 0
    for r, g, b in pixels:
        if r > g + 20 and r > b + 20:
            warm += 1
        elif b > r + 20 and b > g + 20:
            cool += 1
        else:
            neutral += 1

    return {
        "width": width, "height": height,
        "min": min_val, "max": max_val,
        "mean_luma": mean_luma, "luma_std": luma_std,
        "subject_ratio": subject / total,
        "edge_ratio": edges / max(1, edge_samples),
        "warm_ratio": warm / total,
        "cool_ratio": cool / total,
        "bg_color": (int(bg_r), int(bg_g), int(bg_b)),
    }


def print_report(metrics: dict, png_path: pathlib.Path):
    print(f"\n{'='*60}")
    print(f"  Screenshot Analysis: {png_path.name}")
    print(f"{'='*60}")
    print(f"  Resolution:      {metrics['width']}x{metrics['height']}")
    print(f"  Pixel range:     [{metrics['min']}, {metrics['max']}]")
    print(f"  Mean luminance:  {metrics['mean_luma']:.1f}")
    print(f"  Luma std dev:    {metrics['luma_std']:.2f}")
    print(f"  Background:      RGB({metrics['bg_color'][0]}, {metrics['bg_color'][1]}, {metrics['bg_color'][2]})")
    print(f"  Subject ratio:   {metrics['subject_ratio']:.4f}  (non-background pixels)")
    print(f"  Edge ratio:      {metrics['edge_ratio']:.4f}  (detail density)")
    print(f"  Warm/Cool:       {metrics['warm_ratio']:.3f} / {metrics['cool_ratio']:.3f}")
    print()

    issues = []
    if metrics["max"] <= 5:
        issues.append("BLACK SCREEN — max pixel value <= 5")
    if metrics["luma_std"] < 2.0:
        issues.append(f"NEAR SOLID — luma_std={metrics['luma_std']:.2f} (< 2.0)")
    if metrics["subject_ratio"] < 0.01:
        issues.append(f"NO VISIBLE SUBJECT — subject_ratio={metrics['subject_ratio']:.4f} (< 0.01)")
    if metrics["edge_ratio"] < 0.001:
        issues.append(f"NO DETAIL — edge_ratio={metrics['edge_ratio']:.4f} (< 0.001)")
    if metrics["subject_ratio"] > 0.01 and metrics["edge_ratio"] < 0.005:
        issues.append(f"LOW DETAIL — might be flat shading or terrain-only")
    if metrics["subject_ratio"] > 0.5:
        issues.append(f"SUBJECT FILLS FRAME — camera might be too close")

    if issues:
        print("  ISSUES:")
        for issue in issues:
            print(f"    ⚠ {issue}")
    else:
        print("  ✓ Screenshot looks reasonable")
    print()


# ============================================================
# Main
# ============================================================

def main():
    parser = argparse.ArgumentParser(description="自动运行 KF_Framework demo 并截图分析")
    parser.add_argument("--frames", type=int, default=120, help="运行帧数 (默认 120)")
    parser.add_argument("--timeout", type=int, default=30, help="超时秒数 (默认 30)")
    parser.add_argument("--open", action="store_true", help="截图后自动打开查看")
    parser.add_argument("--exe", type=str, default=None, help="引擎可执行文件路径")
    args = parser.parse_args()

    # 路径解析
    script_dir = pathlib.Path(__file__).resolve().parent
    kf_dir = script_dir.parent                          # examples/KF_Framework/
    engine_root = kf_dir.parent.parent                  # DSEngine/
    lua_script = kf_dir / "script" / "main.lua"
    out_dir = kf_dir / "screenshots"
    out_dir.mkdir(parents=True, exist_ok=True)
    png_path = out_dir / "scene_capture.png"

    # 查找引擎可执行文件
    if args.exe:
        exe = pathlib.Path(args.exe)
    else:
        for candidate in [
            engine_root / "bin" / "dsengine_game_release.exe",
            engine_root / "bin" / "dsengine_game_debug.exe",
            engine_root / "bin" / "dsengine_lua_debug.exe",
        ]:
            if candidate.exists():
                exe = candidate
                break
        else:
            print("[ERROR] 未找到引擎可执行文件，请使用 --exe 指定", file=sys.stderr)
            return 1

    if not lua_script.exists():
        print(f"[ERROR] Lua 脚本不存在: {lua_script}", file=sys.stderr)
        return 1

    print(f"[verify_scene] Engine:  {exe}")
    print(f"[verify_scene] Script:  {lua_script}")
    print(f"[verify_scene] Frames:  {args.frames}")
    print(f"[verify_scene] Output:  {png_path}")
    print()

    # 运行引擎
    env = os.environ.copy()
    env["DSE_MAX_FRAMES"] = str(args.frames)
    env["DSE_SCREENSHOT_PATH"] = str(png_path)
    env["DSE_SCREENSHOT_TARGET"] = "main"
    env["DSE_DATA_ROOT"] = str(kf_dir)
    env["DSE_AUTO_BATTLE"] = "1"
    env["DSE_STARTUP_LUA"] = str(lua_script)
    env["DSE_DISABLE_STARTUP_SCENE_REGRESSION"] = "1"

    cmd = [str(exe), f"--script={lua_script}"]
    print(f"[verify_scene] Running: {' '.join(cmd)}")
    print()

    log_path = out_dir / "scene_capture.log"
    try:
        proc = subprocess.run(
            cmd,
            cwd=str(engine_root),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            timeout=args.timeout,
        )
        output = proc.stdout or ""
        log_path.write_text(output, encoding="utf-8", errors="replace")
        print(f"[verify_scene] Exit code: {proc.returncode}")
    except subprocess.TimeoutExpired as exc:
        output = (exc.stdout or "")
        if isinstance(output, bytes):
            output = output.decode("utf-8", errors="replace")
        log_path.write_text(output + "\n[TIMEOUT]\n", encoding="utf-8", errors="replace")
        print(f"[verify_scene] TIMEOUT after {args.timeout}s")
        return 124

    # 打印关键日志行
    for line in output.splitlines():
        if any(k in line for k in ("[KF_Framework]", "Runtime stats", "DSE_SCREENSHOT", "[ERROR]", "Failed")):
            print(f"  LOG: {line.strip()}")
    print()

    # 分析截图
    if not png_path.exists():
        print("[ERROR] 截图文件未生成！引擎可能未到达截图帧。")
        return 2

    file_size = png_path.stat().st_size
    print(f"[verify_scene] Screenshot: {png_path} ({file_size} bytes)")

    try:
        metrics = analyze_screenshot(png_path)
        print_report(metrics, png_path)
    except Exception as exc:
        print(f"[ERROR] 截图解析失败: {exc}")
        return 3

    # 自动打开
    if args.open and sys.platform == "win32":
        os.startfile(str(png_path))

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
