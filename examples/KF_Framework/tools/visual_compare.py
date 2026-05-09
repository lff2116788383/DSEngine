#!/usr/bin/env python3
"""
visual_compare.py — KF_Framework 视觉还原一站式对比工具

合并自: capture_kf_original.py + verify_scene.py + screenshot_compare.py

用法:
    python tools/visual_compare.py                     # 全流程: 截KF + 截DSE + 对比
    python tools/visual_compare.py --kf-only           # 仅截 KF 原版
    python tools/visual_compare.py --dse-only          # 仅截 DSEngine
    python tools/visual_compare.py --compare-only      # 仅对比已有截图
    python tools/visual_compare.py --kf-wait 8         # KF 截图等待秒数
    python tools/visual_compare.py --dse-frames 300    # DSE 运行帧数
    python tools/visual_compare.py --battle             # 截取战斗场景 (默认)
    python tools/visual_compare.py --title              # 截取标题画面

输出:
    screenshots/kf_original_battle.png
    screenshots/dse_battle.png
    screenshots/diff_report.txt
    screenshots/diff_heatmap.png (如果有 PIL)
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import math
import os
import pathlib
import struct
import subprocess
import sys
import time
import zlib

if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")

# ============================================================================
#  PNG 纯 stdlib 编解码
# ============================================================================

def _paeth(a, b, c):
    p = a + b - c
    pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
    if pa <= pb and pa <= pc: return a
    return b if pb <= pc else c

def read_png_rgb(path):
    """解码 8-bit RGB/RGBA PNG → (width, height, [(r,g,b)...])"""
    data = pathlib.Path(path).read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("not a PNG file")
    pos, w, h, bd, ct = 8, None, None, None, None
    raw = b""
    while pos + 8 <= len(data):
        length = struct.unpack(">I", data[pos:pos+4])[0]
        ctype = data[pos+4:pos+8]
        chunk = data[pos+8:pos+8+length]
        pos += 12 + length
        if ctype == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", chunk[:10])
        elif ctype == b"IDAT":
            raw += chunk
        elif ctype == b"IEND":
            break
    if w is None or bd != 8 or ct not in (2, 6):
        raise ValueError(f"unsupported: {bd}bit ct={ct}")
    ch = 4 if ct == 6 else 3
    stride = w * ch
    inflated = zlib.decompress(raw)
    pixels, cursor, prev = [], 0, bytearray(stride)
    for _ in range(h):
        ft = inflated[cursor]; cursor += 1
        row = bytearray(inflated[cursor:cursor+stride]); cursor += stride
        for i in range(stride):
            left = row[i-ch] if i >= ch else 0
            up = prev[i]
            ul = prev[i-ch] if i >= ch else 0
            if   ft == 1: row[i] = (row[i] + left) & 0xFF
            elif ft == 2: row[i] = (row[i] + up) & 0xFF
            elif ft == 3: row[i] = (row[i] + ((left+up)//2)) & 0xFF
            elif ft == 4: row[i] = (row[i] + _paeth(left, up, ul)) & 0xFF
        prev = row
        for x in range(0, stride, ch):
            pixels.append((row[x], row[x+1], row[x+2]))
    return w, h, pixels

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
#  Windows GDI 截图 (来自 capture_kf_original.py)
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
user32.SetForegroundWindow.argtypes = [wt.HWND]
user32.PrintWindow.argtypes = [wt.HWND, wt.HDC, wt.UINT]
user32.PrintWindow.restype = wt.BOOL
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]

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
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [WNDENUMPROC, wt.LPARAM]
user32.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.GetWindowTextLengthW.argtypes = [wt.HWND]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.IsWindowVisible.restype = wt.BOOL
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetWindowThreadProcessId.restype = wt.DWORD
user32.FindWindowExW.restype = wt.HWND
user32.FindWindowExW.argtypes = [wt.HWND, wt.HWND, wt.LPCWSTR, wt.LPCWSTR]
user32.PostMessageW.restype = wt.BOOL
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_ulonglong, ctypes.c_longlong]

BM_CLICK = 0x00F5

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

def dismiss_fullscreen_dialog(pid):
    for _ in range(50):
        hwnd = find_window_by_pid(pid)
        if hwnd:
            btn = user32.FindWindowExW(hwnd, None, "Button", None)
            while btn:
                length = user32.GetWindowTextLengthW(btn)
                if length > 0:
                    buf = ctypes.create_unicode_buffer(length + 1)
                    user32.GetWindowTextW(btn, buf, length + 1)
                    if "否" in buf.value or ("N" in buf.value.upper() and "Y" not in buf.value.upper()):
                        user32.PostMessageW(btn, BM_CLICK, 0, 0)
                        return True
                btn = user32.FindWindowExW(hwnd, btn, "Button", None)
        time.sleep(0.1)
    return False

def bring_to_front(hwnd):
    user32.ShowWindow = user32.ShowWindow
    user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
    user32.BringWindowToTop.argtypes = [wt.HWND]
    user32.ShowWindow(hwnd, 9)  # SW_RESTORE
    user32.ShowWindow(hwnd, 5)  # SW_SHOW
    user32.SetForegroundWindow(hwnd)
    user32.BringWindowToTop(hwnd)
    time.sleep(0.3)
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.3)

PW_CLIENTONLY = 0x1
PW_RENDERFULLCONTENT = 0x2

def capture_window_gdi(hwnd):
    """窗口截图 (纯后台: 不抢焦点, 不要求窗口在最前面)"""
    # 优先 GetClientRect, 回退 GetWindowRect (后台/最小化时仍有效)
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
        print(f"  [capture] ClientRect=0, using WindowRect={w}x{h}")
    if w <= 0 or h <= 0:
        raise RuntimeError(f"Invalid rect: {w}x{h}")

    # PrintWindow: 后台截图, 不需要窗口在前台
    hdc_scr = user32.GetDC(None)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_scr)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_scr, w, h)
    user32.ReleaseDC(None, hdc_scr)
    old = gdi32.SelectObject(hdc_mem, hbmp)
    pw_flags = PW_RENDERFULLCONTENT if use_window_rect else (PW_CLIENTONLY | PW_RENDERFULLCONTENT)
    user32.PrintWindow(hwnd, hdc_mem, pw_flags)

    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = w; bmi.biHeight = -h; bmi.biPlanes = 1; bmi.biBitCount = 32
    buf = ctypes.create_string_buffer(w * h * 4)
    gdi32.GetDIBits(hdc_mem, hbmp, 0, h, buf, ctypes.byref(bmi), DIB_RGB_COLORS)
    gdi32.SelectObject(hdc_mem, old); gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    raw = buf.raw
    rows = []
    for y in range(h):
        row = bytearray(w * 3)
        for x in range(w):
            off = (y * w + x) * 4
            row[x*3] = raw[off+2]; row[x*3+1] = raw[off+1]; row[x*3+2] = raw[off]
        rows.append(bytes(row))
    return w, h, rows

# ============================================================================
#  键盘输入
# ============================================================================

VK_LEFT    = 0x25
VK_RIGHT   = 0x27
VK_SPACE   = 0x20
VK_RETURN  = 0x0D
KEYEVENTF_KEYUP       = 0x0002
KEYEVENTF_SCANCODE    = 0x0008
KEYEVENTF_EXTENDEDKEY = 0x0001
INPUT_KEYBOARD = 1

# VK → scan code 映射 (DirectInput compatible)
_SCAN = {0x25: 0x4B, 0x27: 0x4D, 0x20: 0x39, 0x0D: 0x1C}  # LEFT, RIGHT, SPACE, ENTER
_EXTENDED_VKS = {0x25, 0x26, 0x27, 0x28}  # arrow keys are extended

# 64-bit compatible INPUT structures
ULONG_PTR = ctypes.POINTER(ctypes.c_ulonglong) if ctypes.sizeof(ctypes.c_void_p) == 8 else ctypes.POINTER(ctypes.c_ulong)

class KEYBDINPUT(ctypes.Structure):
    _fields_ = [
        ("wVk", wt.WORD),
        ("wScan", wt.WORD),
        ("dwFlags", wt.DWORD),
        ("time", wt.DWORD),
        ("dwExtraInfo", ULONG_PTR),
    ]

class MOUSEINPUT(ctypes.Structure):
    _fields_ = [
        ("dx", wt.LONG), ("dy", wt.LONG), ("mouseData", wt.DWORD),
        ("dwFlags", wt.DWORD), ("time", wt.DWORD), ("dwExtraInfo", ULONG_PTR),
    ]

class HARDWAREINPUT(ctypes.Structure):
    _fields_ = [("uMsg", wt.DWORD), ("wParamL", wt.WORD), ("wParamH", wt.WORD)]

class INPUT_UNION(ctypes.Union):
    _fields_ = [("ki", KEYBDINPUT), ("mi", MOUSEINPUT), ("hi", HARDWAREINPUT)]

class INPUT(ctypes.Structure):
    _fields_ = [("type", wt.DWORD), ("u", INPUT_UNION)]

user32.SendInput.argtypes = [wt.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
user32.SendInput.restype = wt.UINT

def send_key(hwnd, vk, hold_ms=120):
    """使用 SendInput + scan code 注入键盘 (兼容 DirectInput 8)"""
    bring_to_front(hwnd)
    time.sleep(0.15)
    sc = _SCAN.get(vk, 0)
    ext = KEYEVENTF_EXTENDEDKEY if vk in _EXTENDED_VKS else 0
    # key down
    inp = INPUT()
    inp.type = INPUT_KEYBOARD
    inp.u.ki.wVk = 0  # 0 = use scan code only (better for DirectInput)
    inp.u.ki.wScan = sc
    inp.u.ki.dwFlags = KEYEVENTF_SCANCODE | ext
    inp.u.ki.time = 0
    inp.u.ki.dwExtraInfo = None
    n = user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))
    time.sleep(hold_ms / 1000.0)
    # key up
    inp.u.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP | ext
    user32.SendInput(1, ctypes.byref(inp), ctypes.sizeof(INPUT))
    time.sleep(0.15)
    return n

# ============================================================================
#  Step 1: 截取 KF 原版
# ============================================================================

def capture_kf(exe_path, out_path, wait_title=3.0, battle_mode=True, battle_wait=5.0, manual=False, demo_play=False):
    print(f"\n{'='*60}")
    print(f"  Step 1: Capture KF Original")
    print(f"{'='*60}")

    exe = pathlib.Path(exe_path)
    if not exe.exists():
        print(f"  [SKIP] KF exe not found: {exe}")
        return False

    print(f"  Launching: {exe}")
    env = os.environ.copy()
    proc = subprocess.Popen([str(exe)], cwd=str(exe.parent), env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print(f"  PID: {proc.pid}")

    # 处理全屏对话框
    time.sleep(1.0)
    dismissed = dismiss_fullscreen_dialog(proc.pid)
    print(f"  Fullscreen dialog: {'dismissed' if dismissed else 'not found'}")
    time.sleep(2.0)

    # 等待游戏窗口
    hwnd = None
    for _ in range(150):
        hwnd = find_window_by_pid(proc.pid)
        if hwnd:
            rect = wt.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(rect))
            if rect.right > 400 and rect.bottom > 300:
                break
            hwnd = None
        time.sleep(0.1)

    if not hwnd:
        print("  [ERROR] Game window not found!")
        proc.terminate()
        return False

    rect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    print(f"  Window: {rect.right}x{rect.bottom}")

    print(f"  Waiting {wait_title}s for title to load...")
    time.sleep(wait_title)

    if battle_mode:
        if manual:
            print()
            print("  ╔═══════════════════════════════════════════════════╗")
            print("  ║  请在 KF 窗口中手动操作:                         ║")
            print("  ║  ←/→ 切换 PLAY GAME / DEMO PLAY                ║")
            print("  ║  Space 确认进入游戏                              ║")
            print("  ║  等待战斗场景加载完成后                          ║")
            print("  ║  回到此终端按 Enter 截图                         ║")
            print("  ╚═══════════════════════════════════════════════════╝")
            input("  >> 准备好后按 Enter 截图...")
        else:
            mode_name = "DEMO PLAY" if demo_play else "PLAY GAME"
            print(f"  Auto mode: injecting keys for [{mode_name}]...")
            for attempt in range(3):
                bring_to_front(hwnd)
                time.sleep(0.3)
                if demo_play:
                    # DEMO PLAY: Right → 选中第二项, 然后 Space 确认
                    send_key(hwnd, VK_RIGHT, 150)
                    time.sleep(0.3)
                n = send_key(hwnd, VK_SPACE, 150)
                print(f"    Attempt {attempt+1}: SendInput returned {n} (1=success) [{mode_name}]")
                if n == 1:
                    break
                time.sleep(0.5)
            print(f"  Waiting {battle_wait}s for battle scene to load...")
            time.sleep(battle_wait)

    # 重新搜索窗口 (模式切换可能重建窗口)
    poll = proc.poll()
    if poll is not None:
        print(f"  [ERROR] KF process exited (code={poll})")
        return False
    print(f"  Process alive, searching window...")

    # 纯后台截图: 不抢焦点, 不 ShowWindow
    captured = False
    for attempt in range(80):
        hwnd = find_window_by_pid(proc.pid)
        if hwnd:
            try:
                w, h, rows = capture_window_gdi(hwnd)
                if w > 100 and h > 100:
                    write_png(out_path, w, h, rows)
                    print(f"  Saved: {out_path} ({w}x{h})")
                    captured = True
                    break
            except RuntimeError as e:
                if attempt < 3 or attempt % 20 == 0:
                    print(f"    [{attempt}] {e}, retrying...")
        else:
            if attempt % 20 == 0:
                print(f"    [{attempt}] no visible window found")
        if proc.poll() is not None:
            print(f"  [ERROR] KF exited (code={proc.returncode})")
            return False
        time.sleep(0.2)
    if not captured:
        print("  [ERROR] Failed to capture KF window!")
        proc.terminate()
        return False

    proc.terminate()
    try:
        proc.wait(timeout=5)
    except subprocess.TimeoutExpired:
        proc.kill()

    return True

# ============================================================================
#  Step 2: 截取 DSEngine
# ============================================================================

def capture_dse(engine_root, kf_dir, out_path, frames=300, timeout=60, demo_play=False):
    print(f"\n{'='*60}")
    print(f"  Step 2: Capture DSEngine")
    print(f"{'='*60}")

    exe = None
    for c in ["DSEngine_Game_release.exe", "DSEngine_Game_debug.exe"]:
        p = engine_root / "bin" / c
        if p.exists():
            exe = p; break
    if not exe:
        print("  [ERROR] DSEngine exe not found!")
        return False

    lua_script = kf_dir / "script" / "main.lua"
    env = os.environ.copy()
    env["DSE_MAX_FRAMES"] = str(frames)
    env["DSE_SCREENSHOT_PATH"] = str(out_path)
    env["DSE_SCREENSHOT_TARGET"] = "main"
    env["DSE_DATA_ROOT"] = str(kf_dir)
    env["DSE_AUTO_BATTLE"] = "2" if demo_play else "1"  # 1=PlayGame(no AI), 2=DemoPlay(AI)
    env["DSE_STARTUP_LUA"] = str(lua_script)
    env["DSE_DISABLE_STARTUP_SCENE_REGRESSION"] = "1"

    cmd = [str(exe), f"--script={lua_script}"]
    print(f"  Engine: {exe}")
    print(f"  Frames: {frames}")
    print(f"  Running...")

    try:
        proc = subprocess.run(cmd, cwd=str(engine_root), env=env,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              text=True, errors="replace", timeout=timeout)
        # 检查关键日志
        for line in (proc.stdout or "").splitlines():
            if "Battle start" in line or "DSE_SCREENSHOT_WRITTEN" in line:
                print(f"  LOG: {line.strip()}")
    except subprocess.TimeoutExpired:
        print(f"  [WARN] Timeout after {timeout}s")

    if out_path.exists():
        print(f"  Saved: {out_path} ({out_path.stat().st_size} bytes)")
        return True
    else:
        print("  [ERROR] Screenshot not generated!")
        return False

# ============================================================================
#  Step 3: 对比
# ============================================================================

def compare_screenshots(kf_path, dse_path, report_path, heatmap_path=None):
    print(f"\n{'='*60}")
    print(f"  Step 3: Compare Screenshots")
    print(f"{'='*60}")

    if not pathlib.Path(kf_path).exists():
        print(f"  [ERROR] KF screenshot not found: {kf_path}")
        return
    if not pathlib.Path(dse_path).exists():
        print(f"  [ERROR] DSE screenshot not found: {dse_path}")
        return

    kf_w, kf_h, kf_px = read_png_rgb(kf_path)
    dse_w, dse_h, dse_px = read_png_rgb(dse_path)
    print(f"  KF  size: {kf_w}x{kf_h}")
    print(f"  DSE size: {dse_w}x{dse_h}")

    # 统一到较小尺寸 (简单裁剪)
    w = min(kf_w, dse_w)
    h = min(kf_h, dse_h)

    # 逐像素对比
    total = w * h
    sum_sq = 0.0
    sum_abs = [0.0, 0.0, 0.0]
    max_diff = 0
    per_pixel_err = []

    # 亮度分析
    kf_luma_sum = 0.0
    dse_luma_sum = 0.0
    dark_kf = 0   # KF 暗像素 (<30)
    dark_dse = 0  # DSE 暗像素 (<30)
    bright_kf = 0
    bright_dse = 0

    buckets = [0]*6  # <5, <10, <20, <40, <80, >=80

    for y in range(h):
        for x in range(w):
            kr, kg, kb = kf_px[y * kf_w + x]
            dr, dg, db = dse_px[y * dse_w + x]
            diff_r = abs(kr - dr)
            diff_g = abs(kg - dg)
            diff_b = abs(kb - db)
            sum_abs[0] += diff_r
            sum_abs[1] += diff_g
            sum_abs[2] += diff_b
            sq = diff_r**2 + diff_g**2 + diff_b**2
            sum_sq += sq
            pxe = math.sqrt(sq / 3.0)
            per_pixel_err.append(pxe)
            md = max(diff_r, diff_g, diff_b)
            if md > max_diff: max_diff = md

            if pxe < 5:    buckets[0] += 1
            elif pxe < 10: buckets[1] += 1
            elif pxe < 20: buckets[2] += 1
            elif pxe < 40: buckets[3] += 1
            elif pxe < 80: buckets[4] += 1
            else:          buckets[5] += 1

            kf_l = 0.2126*kr + 0.7152*kg + 0.0722*kb
            dse_l = 0.2126*dr + 0.7152*dg + 0.0722*db
            kf_luma_sum += kf_l
            dse_luma_sum += dse_l
            if kf_l < 30: dark_kf += 1
            if dse_l < 30: dark_dse += 1
            if kf_l > 200: bright_kf += 1
            if dse_l > 200: bright_dse += 1

    rmse = math.sqrt(sum_sq / (total * 3))
    mean_abs = [s / total for s in sum_abs]
    kf_mean_luma = kf_luma_sum / total
    dse_mean_luma = dse_luma_sum / total

    # 报告
    lines = []
    lines.append("=" * 60)
    lines.append("  KF_Framework Visual Comparison Report")
    lines.append("=" * 60)
    lines.append(f"  Compare area:      {w}x{h}")
    lines.append(f"  RMSE (global):     {rmse:.2f} / 255")
    lines.append(f"  Mean |diff| R:     {mean_abs[0]:.2f}")
    lines.append(f"  Mean |diff| G:     {mean_abs[1]:.2f}")
    lines.append(f"  Mean |diff| B:     {mean_abs[2]:.2f}")
    lines.append(f"  Max channel diff:  {max_diff}")
    lines.append(f"")
    lines.append(f"  Pixel error distribution:")
    cum = 0
    for i, (label, count) in enumerate([
        ("<5",  buckets[0]), ("<10", buckets[1]), ("<20", buckets[2]),
        ("<40", buckets[3]), ("<80", buckets[4]), (">=80", buckets[5])
    ]):
        cum += count
        lines.append(f"    err {label:>4}:  {count:>8} px  ({count/total*100:5.1f}%)  cum {cum/total*100:5.1f}%")
    lines.append(f"")
    lines.append(f"  Luminance:")
    lines.append(f"    KF  mean:  {kf_mean_luma:.1f}")
    lines.append(f"    DSE mean:  {dse_mean_luma:.1f}")
    lines.append(f"    Diff:      {dse_mean_luma - kf_mean_luma:+.1f}")
    lines.append(f"    KF  dark(<30):   {dark_kf/total*100:.1f}%   bright(>200): {bright_kf/total*100:.1f}%")
    lines.append(f"    DSE dark(<30):   {dark_dse/total*100:.1f}%   bright(>200): {bright_dse/total*100:.1f}%")
    lines.append(f"")

    # 诊断
    issues = []
    if rmse > 60:
        issues.append(f"HIGH RMSE ({rmse:.1f}) — 整体差异大，可能缺少 gamma 校正或 tone mapping 不匹配")
    if abs(dse_mean_luma - kf_mean_luma) > 30:
        if dse_mean_luma < kf_mean_luma:
            issues.append(f"DSE 比 KF 暗 {kf_mean_luma - dse_mean_luma:.0f} luma — 可能缺少 gamma 编码")
        else:
            issues.append(f"DSE 比 KF 亮 {dse_mean_luma - kf_mean_luma:.0f} luma — 可能 tone mapping 过亮")
    if dark_dse / total > 0.5 and dark_kf / total < 0.3:
        issues.append("DSE 暗部像素过多 — 地面/场景过暗，检查 gamma 或光照强度")
    if buckets[5] / total > 0.3:
        issues.append(f">=80 误差像素占 {buckets[5]/total*100:.0f}% — 严重色差，需检查 shader 或后处理")

    if issues:
        lines.append("  ISSUES:")
        for issue in issues:
            lines.append(f"    ⚠ {issue}")
    else:
        lines.append("  ✓ Visual match is reasonable (RMSE < 60, luminance close)")

    lines.append("=" * 60)
    report = "\n".join(lines)
    print(report)

    # 保存报告
    pathlib.Path(report_path).write_text(report, encoding="utf-8")
    print(f"\n  Report saved: {report_path}")

    # 尝试生成 heatmap (需要 PIL)
    if heatmap_path:
        try:
            _write_diff_heatmap(w, h, per_pixel_err, kf_px, kf_w, dse_px, dse_w, heatmap_path)
        except Exception as e:
            print(f"  [WARN] Heatmap generation failed: {e}")

def _write_diff_heatmap(w, h, errors, kf_px, kf_w, dse_px, dse_w, out_path):
    """生成 diff heatmap PNG (纯 stdlib, 红色=差异大)"""
    rows = []
    max_err = max(errors) if errors else 1.0
    scale = 255.0 / max(max_err, 1.0)
    for y in range(h):
        row = bytearray(w * 3)
        for x in range(w):
            e = errors[y * w + x]
            intensity = min(255, int(e * scale))
            # 红-黄-白 heat map
            if intensity < 85:
                row[x*3] = intensity * 3; row[x*3+1] = 0; row[x*3+2] = 0
            elif intensity < 170:
                row[x*3] = 255; row[x*3+1] = (intensity - 85) * 3; row[x*3+2] = 0
            else:
                row[x*3] = 255; row[x*3+1] = 255; row[x*3+2] = (intensity - 170) * 3
            # 误差<3 的像素显示暗灰原图
            if e < 3:
                kr, kg, kb = kf_px[y * kf_w + x]
                row[x*3] = kr // 3; row[x*3+1] = kg // 3; row[x*3+2] = kb // 3
        rows.append(bytes(row))
    write_png(out_path, w, h, rows)
    print(f"  Heatmap saved: {out_path}")

# ============================================================================
#  Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="KF_Framework 视觉还原对比工具")
    parser.add_argument("--kf-only", action="store_true", help="仅截取 KF 原版")
    parser.add_argument("--dse-only", action="store_true", help="仅截取 DSEngine")
    parser.add_argument("--compare-only", action="store_true", help="仅对比已有截图")
    parser.add_argument("--title", action="store_true", help="截取标题画面 (默认: 战斗)")
    parser.add_argument("--kf-exe", type=str,
                        default=r"C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\KF_Framework_Release.exe")
    parser.add_argument("--kf-wait", type=float, default=4.0, help="KF Title 加载等待秒数")
    parser.add_argument("--battle-wait", type=float, default=10.0, help="KF 进入战斗后等待秒数 (需要足够时间完成fade+加载)")
    parser.add_argument("--demo-play", action="store_true", help="KF 进入 DEMO PLAY 模式 (而非 PLAY GAME)")
    parser.add_argument("--manual", action="store_true", help="KF 手动操作模式 (DirectInput 注入失败时使用)")
    parser.add_argument("--auto-input", action="store_true", default=True, help="KF 自动键盘注入模式 (默认)")
    parser.add_argument("--dse-frames", type=int, default=180, help="DSEngine 运行帧数")
    parser.add_argument("--timeout", type=int, default=60, help="DSEngine 超时秒数")
    args = parser.parse_args()

    script_dir = pathlib.Path(__file__).resolve().parent
    kf_dir = script_dir.parent
    engine_root = kf_dir.parent.parent
    out_dir = kf_dir / "screenshots"
    out_dir.mkdir(parents=True, exist_ok=True)

    battle = not args.title
    suffix = "battle" if battle else "title"
    kf_png = out_dir / f"kf_original_{suffix}.png"
    dse_png = out_dir / f"dse_{suffix}.png"
    report = out_dir / "diff_report.txt"
    heatmap = out_dir / "diff_heatmap.png"

    do_kf = not args.dse_only and not args.compare_only
    do_dse = not args.kf_only and not args.compare_only
    do_cmp = not args.kf_only and not args.dse_only

    if do_kf:
        capture_kf(args.kf_exe, kf_png, args.kf_wait, battle, args.battle_wait, args.manual, args.demo_play)

    if do_dse:
        capture_dse(engine_root, kf_dir, dse_png, args.dse_frames, args.timeout, args.demo_play)

    if do_cmp:
        compare_screenshots(kf_png, dse_png, report, heatmap)

    return 0

if __name__ == "__main__":
    raise SystemExit(main())
