#!/usr/bin/env python3
"""
capture_lua_demos.py — Lua 3D Demo 自动截图验证工具

参考 examples/KF_Framework/tools/visual_compare.py 的 GDI 后台截图技术。

用法:
    python samples/lua/tools/capture_lua_demos.py                       # 截取全部 3 个 demo
    python samples/lua/tools/capture_lua_demos.py --demo 3d_cloth       # 只截布料
    python samples/lua/tools/capture_lua_demos.py --demo 3d_fluid       # 只截流体
    python samples/lua/tools/capture_lua_demos.py --demo 3d_fracture    # 只截破碎
    python samples/lua/tools/capture_lua_demos.py --wait 6              # 等待 6 秒再截图
    python samples/lua/tools/capture_lua_demos.py --out screenshots     # 输出目录

输出:
    screenshots/3d_fracture.png
    screenshots/3d_cloth.png
    screenshots/3d_fluid.png
    screenshots/demo_report.txt
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import pathlib
import struct
import subprocess
import sys
import time
import zlib

if sys.platform != "win32":
    print("This script only runs on Windows (requires GDI).")
    sys.exit(1)

os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")

# ============================================================================
#  PNG 纯 stdlib 编码
# ============================================================================

def write_png(path, width, height, rows):
    """写入 RGB PNG (rows = [bytes(width*3), ...])"""
    def chunk(ct, d):
        c = ct + d
        return struct.pack(">I", len(d)) + c + struct.pack(">I", zlib.crc32(c) & 0xFFFFFFFF)
    raw_data = b""
    for row in rows:
        raw_data += b"\x00" + row
    compressed = zlib.compress(raw_data, 9)
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))

# ============================================================================
#  Windows GDI 截图 (来自 visual_compare.py)
# ============================================================================

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32

user32.FindWindowW.restype = wt.HWND
user32.FindWindowW.argtypes = [wt.LPCWSTR, wt.LPCWSTR]
user32.GetWindowRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetClientRect.argtypes = [wt.HWND, ctypes.POINTER(wt.RECT)]
user32.GetDC.restype = wt.HDC
user32.GetDC.argtypes = [wt.HWND]
user32.ReleaseDC.argtypes = [wt.HWND, wt.HDC]
user32.PrintWindow.argtypes = [wt.HWND, wt.HDC, wt.UINT]
user32.PrintWindow.restype = wt.BOOL
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.IsWindowVisible.restype = wt.BOOL
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetWindowThreadProcessId.restype = wt.DWORD
user32.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.GetWindowTextLengthW.argtypes = [wt.HWND]
user32.GetWindowTextLengthW.restype = ctypes.c_int

gdi32.CreateCompatibleDC.restype = wt.HDC
gdi32.CreateCompatibleDC.argtypes = [wt.HDC]
gdi32.CreateCompatibleBitmap.restype = wt.HBITMAP
gdi32.CreateCompatibleBitmap.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int]
gdi32.SelectObject.restype = wt.HGDIOBJ
gdi32.SelectObject.argtypes = [wt.HDC, wt.HGDIOBJ]
gdi32.BitBlt.restype = wt.BOOL
gdi32.BitBlt.argtypes = [wt.HDC, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int,
                         wt.HDC, ctypes.c_int, ctypes.c_int, wt.DWORD]
gdi32.GetDIBits.restype = ctypes.c_int
gdi32.GetDIBits.argtypes = [wt.HDC, wt.HBITMAP, wt.UINT, wt.UINT,
                            ctypes.c_void_p, ctypes.c_void_p, wt.UINT]
gdi32.DeleteObject.argtypes = [wt.HGDIOBJ]
gdi32.DeleteDC.argtypes = [wt.HDC]

SRCCOPY = 0x00CC0020
DIB_RGB_COLORS = 0
PW_CLIENTONLY = 0x1
PW_RENDERFULLCONTENT = 0x2

WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [WNDENUMPROC, wt.LPARAM]

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wt.DWORD), ("biWidth", wt.LONG), ("biHeight", wt.LONG),
        ("biPlanes", wt.WORD), ("biBitCount", wt.WORD), ("biCompression", wt.DWORD),
        ("biSizeImage", wt.DWORD), ("biXPelsPerMeter", wt.LONG), ("biYPelsPerMeter", wt.LONG),
        ("biClrUsed", wt.DWORD), ("biClrImportant", wt.DWORD),
    ]

def find_window_by_pid(pid):
    result = []
    def cb(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            pid_out = wt.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(pid_out))
            if pid_out.value == pid:
                result.append(hwnd)
        return True
    user32.EnumWindows(WNDENUMPROC(cb), 0)
    return result[0] if result else None

def get_window_title(hwnd):
    length = user32.GetWindowTextLengthW(hwnd)
    if length <= 0:
        return ""
    buf = ctypes.create_unicode_buffer(length + 1)
    user32.GetWindowTextW(hwnd, buf, length + 1)
    return buf.value

def capture_window_gdi(hwnd):
    """GDI 后台窗口截图 -> (width, height, [bytes(row_rgb), ...])"""
    crect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(crect))
    w, h = crect.right, crect.bottom
    use_window_rect = False
    if w <= 0 or h <= 0:
        wrect = wt.RECT()
        user32.GetWindowRect(hwnd, ctypes.byref(wrect))
        w = wrect.right - wrect.left
        h = wrect.bottom - wrect.top
        use_window_rect = True
    if w <= 0 or h <= 0:
        raise RuntimeError(f"Invalid rect: {w}x{h}")

    hdc_scr = user32.GetDC(None)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_scr)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_scr, w, h)
    user32.ReleaseDC(None, hdc_scr)
    old = gdi32.SelectObject(hdc_mem, hbmp)
    pw_flags = PW_RENDERFULLCONTENT if use_window_rect else (PW_CLIENTONLY | PW_RENDERFULLCONTENT)
    user32.PrintWindow(hwnd, hdc_mem, pw_flags)

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w
    bmi.biHeight = -h
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), DIB_RGB_COLORS)
    gdi32.SelectObject(hdc_mem, old)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    raw = buf.raw
    rows = []
    for y in range(h):
        row = bytearray(w * 3)
        for x in range(w):
            off = (y * w + x) * 4
            row[x * 3] = raw[off + 2]      # R
            row[x * 3 + 1] = raw[off + 1]  # G
            row[x * 3 + 2] = raw[off]      # B
        rows.append(bytes(row))
    return w, h, rows

# ============================================================================
#  Screenshot analysis (basic)
# ============================================================================

def analyze_screenshot(rows, w, h):
    """Basic analysis: average luminance, non-black pixel ratio, color stats."""
    total = w * h
    luma_sum = 0.0
    non_black = 0
    r_sum, g_sum, b_sum = 0, 0, 0

    for y in range(h):
        row = rows[y]
        for x in range(w):
            r, g, b = row[x * 3], row[x * 3 + 1], row[x * 3 + 2]
            luma = 0.2126 * r + 0.7152 * g + 0.0722 * b
            luma_sum += luma
            r_sum += r
            g_sum += g
            b_sum += b
            if r > 5 or g > 5 or b > 5:
                non_black += 1

    mean_luma = luma_sum / total if total > 0 else 0
    non_black_pct = non_black / total * 100 if total > 0 else 0
    mean_r = r_sum / total
    mean_g = g_sum / total
    mean_b = b_sum / total

    return {
        "mean_luma": mean_luma,
        "non_black_pct": non_black_pct,
        "mean_rgb": (mean_r, mean_g, mean_b),
    }

# ============================================================================
#  Demo capture
# ============================================================================

DEMOS = ["3d_fracture", "3d_cloth", "3d_fluid"]

DEMO_EXPECT = {
    "3d_fracture": "Destructible boxes (6 cubes on ground, should show colored geometry)",
    "3d_cloth":    "Cloth draped from top-left/right corners, colliding with sphere, wind effect",
    "3d_fluid":    "Blue particles from 4 emitters, falling under gravity onto ground plane",
}

def set_config_entry(config_path, entry_name):
    """Modify config.lua to set game_entry."""
    text = config_path.read_text(encoding="utf-8")
    import re
    new_text = re.sub(r'game_entry\s*=\s*"[^"]*"', f'game_entry="{entry_name}"', text)
    config_path.write_text(new_text, encoding="utf-8")

def capture_demo(engine_root, demo_name, out_path, wait_seconds=5.0, timeout=30.0):
    """Launch DSEngine, wait, capture GDI screenshot, terminate."""
    print(f"\n{'=' * 60}")
    print(f"  Capturing: {demo_name}")
    print(f"{'=' * 60}")

    bin_dir = engine_root / "bin"
    exe = bin_dir / "dsengine_lua_release.exe"
    if not exe.exists():
        exe = bin_dir / "dsengine_lua_debug.exe"
    if not exe.exists():
        print(f"  [ERROR] Executable not found in {bin_dir}")
        return None

    # Switch config
    config_bin = bin_dir / "samples" / "lua" / "config.lua"
    config_src = engine_root / "samples" / "lua" / "config.lua"
    for cfg in [config_bin, config_src]:
        if cfg.exists():
            set_config_entry(cfg, demo_name)
            print(f"  Config: {cfg.name} -> game_entry=\"{demo_name}\"")

    # Launch
    print(f"  Launching: {exe.name}")
    proc = subprocess.Popen(
        [str(exe)],
        cwd=str(bin_dir),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
    )
    print(f"  PID: {proc.pid}")

    # Wait for window
    hwnd = None
    start = time.time()
    while time.time() - start < timeout:
        hwnd = find_window_by_pid(proc.pid)
        if hwnd:
            crect = wt.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(crect))
            if crect.right > 100 and crect.bottom > 100:
                break
            hwnd = None
        if proc.poll() is not None:
            print(f"  [ERROR] Process exited early (code={proc.returncode})")
            return None
        time.sleep(0.2)

    if not hwnd:
        print(f"  [ERROR] Window not found within {timeout}s")
        proc.terminate()
        return None

    title = get_window_title(hwnd)
    crect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(crect))
    print(f"  Window: \"{title}\" {crect.right}x{crect.bottom}")

    # Wait for scene to stabilize
    print(f"  Waiting {wait_seconds}s for scene to stabilize...")
    time.sleep(wait_seconds)

    # Check process still alive
    if proc.poll() is not None:
        print(f"  [ERROR] Process died during wait (code={proc.returncode})")
        return None

    # Capture
    result = None
    for attempt in range(5):
        try:
            w, h, rows = capture_window_gdi(hwnd)
            if w > 100 and h > 100:
                write_png(str(out_path), w, h, rows)
                print(f"  Saved: {out_path} ({w}x{h}, {out_path.stat().st_size} bytes)")
                result = (w, h, rows)
                break
        except RuntimeError as e:
            print(f"    Attempt {attempt + 1}: {e}")
            time.sleep(0.5)

    if result is None:
        print("  [ERROR] Failed to capture screenshot!")

    # Collect some stdout lines
    proc.terminate()
    try:
        stdout, _ = proc.communicate(timeout=5)
        stdout_text = stdout.decode("utf-8", errors="replace") if stdout else ""
        # Extract interesting log lines
        interesting = []
        for line in stdout_text.splitlines():
            if any(kw in line for kw in ["error", "ERROR", "Demo", "assert", "ASSERT"]):
                interesting.append(line.strip())
        if interesting:
            print(f"  Log highlights:")
            for line in interesting[:10]:
                print(f"    {line}")
    except subprocess.TimeoutExpired:
        proc.kill()

    return result

# ============================================================================
#  Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="Lua 3D Demo Screenshot Tool")
    parser.add_argument("--demo", type=str, default=None,
                        help="Single demo to capture (e.g. 3d_cloth). Default: all")
    parser.add_argument("--wait", type=float, default=5.0,
                        help="Seconds to wait before capturing (default: 5)")
    parser.add_argument("--timeout", type=float, default=30.0,
                        help="Max seconds to wait for window (default: 30)")
    parser.add_argument("--out", type=str, default="screenshots",
                        help="Output directory (default: screenshots)")
    parser.add_argument("--restore", action="store_true", default=True,
                        help="Restore config.lua after capture (default: True)")
    args = parser.parse_args()

    script_dir = pathlib.Path(__file__).resolve().parent
    engine_root = script_dir.parent.parent.parent  # samples/lua/tools -> samples/lua -> samples -> engine_root
    out_dir = engine_root / args.out
    out_dir.mkdir(parents=True, exist_ok=True)

    demos = [args.demo] if args.demo else DEMOS
    results = {}

    print(f"DSEngine Lua Demo Screenshot Tool")
    print(f"Engine root: {engine_root}")
    print(f"Output dir:  {out_dir}")
    print(f"Demos:       {', '.join(demos)}")
    print(f"Wait:        {args.wait}s")

    for demo in demos:
        out_path = out_dir / f"{demo}.png"
        result = capture_demo(engine_root, demo, out_path, args.wait, args.timeout)
        if result:
            w, h, rows = result
            stats = analyze_screenshot(rows, w, h)
            results[demo] = {"size": f"{w}x{h}", "file": str(out_path), **stats}
        else:
            results[demo] = None

    # Restore config
    if args.restore:
        default_entry = "phase1_2d_physics_showcase"
        for cfg_rel in ["bin/samples/lua/config.lua", "samples/lua/config.lua"]:
            cfg = engine_root / cfg_rel
            if cfg.exists():
                set_config_entry(cfg, default_entry)
        print(f"\n  Config restored to \"{default_entry}\"")

    # Report
    print(f"\n{'=' * 60}")
    print(f"  Demo Screenshot Report")
    print(f"{'=' * 60}")

    report_lines = []
    all_ok = True
    for demo in demos:
        r = results.get(demo)
        expect = DEMO_EXPECT.get(demo, "")
        report_lines.append(f"\n  [{demo}]")
        report_lines.append(f"    Expected: {expect}")
        if r is None:
            report_lines.append(f"    Status:   FAILED (no screenshot)")
            all_ok = False
        else:
            report_lines.append(f"    Size:     {r['size']}")
            report_lines.append(f"    File:     {r['file']}")
            report_lines.append(f"    Luma:     {r['mean_luma']:.1f}")
            report_lines.append(f"    Non-black: {r['non_black_pct']:.1f}%")
            mr, mg, mb = r["mean_rgb"]
            report_lines.append(f"    Mean RGB: ({mr:.0f}, {mg:.0f}, {mb:.0f})")

            # Basic sanity checks
            issues = []
            if r["non_black_pct"] < 5:
                issues.append("Almost entirely black - scene may not be rendering")
            if r["mean_luma"] < 3:
                issues.append("Very dark - possible rendering failure")
            if r["mean_luma"] > 240:
                issues.append("Very bright - possible clear-color only (no geometry)")

            if demo == "3d_fluid" and r["mean_luma"] < 5:
                issues.append("Fluid demo too dark - particles may not be visible")
            if demo == "3d_cloth" and r["non_black_pct"] < 10:
                issues.append("Cloth demo mostly black - mesh may not be updating")

            if issues:
                for issue in issues:
                    report_lines.append(f"    WARNING: {issue}")
                all_ok = False
            else:
                report_lines.append(f"    Status:   OK (basic checks passed)")

    report = "\n".join(report_lines)
    print(report)

    report_path = out_dir / "demo_report.txt"
    report_path.write_text(report, encoding="utf-8")
    print(f"\n  Report saved: {report_path}")

    if all_ok:
        print(f"\n  All demos captured successfully!")
    else:
        print(f"\n  Some demos had issues - please check screenshots visually.")

    return 0 if all_ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
