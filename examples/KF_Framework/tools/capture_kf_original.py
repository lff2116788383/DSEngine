#!/usr/bin/env python3
"""
capture_kf_original.py — 启动原版 KF_Framework exe 并截取 Title/Result 画面

用法:
    python tools/capture_kf_original.py                  # 截取 Title 画面 (默认等 3 秒)
    python tools/capture_kf_original.py --wait 5         # 等待 5 秒后截图
    python tools/capture_kf_original.py --result         # 手动进入 Result 后截图 (等待按键)

输出:
    screenshots/kf_original_title.png
    screenshots/kf_original_result.png (--result 模式)

依赖: Windows only (ctypes + GDI), 无需 PIL/pyautogui
"""

import argparse
import ctypes
import ctypes.wintypes as wt
import os
import struct
import subprocess
import sys
import time
import zlib
from pathlib import Path

# ============================================================================
#  Windows API 定义
# ============================================================================

user32 = ctypes.windll.user32
gdi32 = ctypes.windll.gdi32
kernel32 = ctypes.windll.kernel32

# Window functions
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

# GDI functions
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
PW_CLIENTONLY = 1
PW_RENDERFULLCONTENT = 2
DIB_RGB_COLORS = 0

# EnumWindows callback
WNDENUMPROC = ctypes.WINFUNCTYPE(wt.BOOL, wt.HWND, wt.LPARAM)
user32.EnumWindows.argtypes = [WNDENUMPROC, wt.LPARAM]
user32.GetWindowTextW.argtypes = [wt.HWND, wt.LPWSTR, ctypes.c_int]
user32.GetWindowTextLengthW.argtypes = [wt.HWND]
user32.GetWindowTextLengthW.restype = ctypes.c_int
user32.IsWindowVisible.argtypes = [wt.HWND]
user32.IsWindowVisible.restype = wt.BOOL
user32.GetWindowThreadProcessId.argtypes = [wt.HWND, ctypes.POINTER(wt.DWORD)]
user32.GetWindowThreadProcessId.restype = wt.DWORD

# Dialog / child window
user32.FindWindowExW.restype = wt.HWND
user32.FindWindowExW.argtypes = [wt.HWND, wt.HWND, wt.LPCWSTR, wt.LPCWSTR]
user32.PostMessageW.restype = wt.BOOL
user32.PostMessageW.argtypes = [wt.HWND, wt.UINT, ctypes.c_ulonglong, ctypes.c_longlong]
user32.ClientToScreen.argtypes = [wt.HWND, ctypes.POINTER(wt.POINT)]

BM_CLICK = 0x00F5
WM_CLOSE = 0x0010


# ============================================================================
#  BITMAPINFOHEADER
# ============================================================================

class BITMAPINFOHEADER(ctypes.Structure):
    _fields_ = [
        ("biSize", wt.DWORD),
        ("biWidth", wt.LONG),
        ("biHeight", wt.LONG),
        ("biPlanes", wt.WORD),
        ("biBitCount", wt.WORD),
        ("biCompression", wt.DWORD),
        ("biSizeImage", wt.DWORD),
        ("biXPelsPerMeter", wt.LONG),
        ("biYPelsPerMeter", wt.LONG),
        ("biClrUsed", wt.DWORD),
        ("biClrImportant", wt.DWORD),
    ]


# ============================================================================
#  窗口查找
# ============================================================================

def find_window_by_pid(pid: int) -> int:
    """根据进程 ID 查找主窗口句柄"""
    result = []

    def callback(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            proc_id = wt.DWORD()
            user32.GetWindowThreadProcessId(hwnd, ctypes.byref(proc_id))
            if proc_id.value == pid:
                result.append(hwnd)
        return True

    user32.EnumWindows(WNDENUMPROC(callback), 0)
    return result[0] if result else None


def find_window_by_title(title_substr: str) -> int:
    """根据窗口标题子串查找"""
    result = []

    def callback(hwnd, _):
        if user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            if length > 0:
                buf = ctypes.create_unicode_buffer(length + 1)
                user32.GetWindowTextW(hwnd, buf, length + 1)
                if title_substr.lower() in buf.value.lower():
                    result.append(hwnd)
        return True

    user32.EnumWindows(WNDENUMPROC(callback), 0)
    return result[0] if result else None


# ============================================================================
#  窗口截图
# ============================================================================

def dismiss_fullscreen_dialog(pid: int) -> bool:
    """查找并关闭“フルスクリーンモードで起動しますか？”对话框，点击否(N)"""
    for _ in range(50):  # 最多等 5 秒
        hwnd = find_window_by_pid(pid)
        if hwnd:
            # 遍历所有 Button 子窗口
            btn = user32.FindWindowExW(hwnd, None, "Button", None)
            while btn:
                length = user32.GetWindowTextLengthW(btn)
                if length > 0:
                    buf = ctypes.create_unicode_buffer(length + 1)
                    user32.GetWindowTextW(btn, buf, length + 1)
                    text = buf.value
                    if "否" in text or ("N" in text.upper() and "Y" not in text.upper()):
                        print(f"[capture] Found button '{text}', clicking...")
                        user32.PostMessageW(btn, BM_CLICK, 0, 0)
                        return True
                btn = user32.FindWindowExW(hwnd, btn, "Button", None)
        time.sleep(0.1)
    return False


def bring_to_front(hwnd):
    """确保窗口在最前面且未被遮挡"""
    SW_SHOW = 5
    SW_RESTORE = 9
    user32.ShowWindow.argtypes = [wt.HWND, ctypes.c_int]
    user32.BringWindowToTop = user32.BringWindowToTop
    user32.BringWindowToTop.argtypes = [wt.HWND]
    user32.ShowWindow(hwnd, SW_RESTORE)
    user32.ShowWindow(hwnd, SW_SHOW)
    user32.SetForegroundWindow(hwnd)
    user32.BringWindowToTop(hwnd)
    time.sleep(0.5)
    # 再次确保
    user32.SetForegroundWindow(hwnd)
    time.sleep(0.5)


def capture_window(hwnd) -> tuple:
    """截取窗口客户区 (使用屏幕 DC + BitBlt, 兼容 DirectX 9)"""
    # 将窗口置前
    bring_to_front(hwnd)

    # 获取客户区大小
    crect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(crect))
    width = crect.right
    height = crect.bottom

    if width <= 0 or height <= 0:
        raise RuntimeError(f"Invalid client rect: {width}x{height}")

    # 计算客户区在屏幕上的位置
    pt = wt.POINT(0, 0)
    user32.ClientToScreen(hwnd, ctypes.byref(pt))
    screen_x = pt.x
    screen_y = pt.y

    # 从屏幕 DC 截取 (DX9 渲染的内容在屏幕 DC 上可见)
    hdc_screen = user32.GetDC(None)
    hdc_mem = gdi32.CreateCompatibleDC(hdc_screen)
    hbmp = gdi32.CreateCompatibleBitmap(hdc_screen, width, height)
    old_bmp = gdi32.SelectObject(hdc_mem, hbmp)

    gdi32.BitBlt(hdc_mem, 0, 0, width, height, hdc_screen, screen_x, screen_y, SRCCOPY)

    # 读取像素数据
    bmi = BITMAPINFOHEADER()
    bmi.biSize = ctypes.sizeof(BITMAPINFOHEADER)
    bmi.biWidth = width
    bmi.biHeight = -height  # top-down
    bmi.biPlanes = 1
    bmi.biBitCount = 32
    bmi.biCompression = 0  # BI_RGB

    buf_size = width * height * 4
    buf = ctypes.create_string_buffer(buf_size)
    gdi32.GetDIBits(hdc_mem, hbmp, 0, height, buf, ctypes.byref(bmi), DIB_RGB_COLORS)

    # 清理
    gdi32.SelectObject(hdc_mem, old_bmp)
    gdi32.DeleteObject(hbmp)
    gdi32.DeleteDC(hdc_mem)
    user32.ReleaseDC(None, hdc_screen)

    # BGRA → RGB rows
    raw = buf.raw
    rows = []
    for y in range(height):
        row = bytearray(width * 3)
        for x in range(width):
            off = (y * width + x) * 4
            row[x * 3] = raw[off + 2]      # R
            row[x * 3 + 1] = raw[off + 1]  # G
            row[x * 3 + 2] = raw[off]      # B
        rows.append(bytes(row))

    return width, height, rows


# ============================================================================
#  PNG 写入 (纯 stdlib)
# ============================================================================

def write_png(path: Path, width: int, height: int, rows: list):
    """将 RGB 行数据写入 PNG 文件"""
    def chunk(chunk_type: bytes, data: bytes) -> bytes:
        c = chunk_type + data
        crc = zlib.crc32(c) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + c + struct.pack(">I", crc)

    # IHDR
    ihdr_data = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    # IDAT - filter each row with None filter (0)
    raw_data = b""
    for row in rows:
        raw_data += b"\x00" + row  # filter byte = 0 (None)
    compressed = zlib.compress(raw_data, 9)

    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr_data))
        f.write(chunk(b"IDAT", compressed))
        f.write(chunk(b"IEND", b""))

    print(f"[capture] Saved: {path} ({path.stat().st_size} bytes, {width}x{height})")


# ============================================================================
#  Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="截取原版 KF_Framework exe 窗口画面")
    parser.add_argument("--exe", type=str,
                        default=r"C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\KF_Framework_Release.exe",
                        help="原版 exe 路径")
    parser.add_argument("--wait", type=float, default=3.0,
                        help="启动后等待秒数 (默认 3.0, 让 Title 画面完全加载)")
    parser.add_argument("--result", action="store_true",
                        help="等待用户手动进入 Result 画面后再截图")
    parser.add_argument("--no-launch", action="store_true",
                        help="不启动 exe, 仅截取已运行的窗口")
    parser.add_argument("--out-name", type=str, default=None,
                        help="输出文件名 (不含路径)")
    args = parser.parse_args()

    script_dir = Path(__file__).resolve().parent
    kf_dir = script_dir.parent
    out_dir = kf_dir / "screenshots"
    out_dir.mkdir(parents=True, exist_ok=True)

    exe_path = Path(args.exe)
    exe_dir = exe_path.parent
    proc = None

    if not args.no_launch:
        if not exe_path.exists():
            print(f"[ERROR] exe 不存在: {exe_path}", file=sys.stderr)
            return 1
        print(f"[capture] Launching: {exe_path}")
        proc = subprocess.Popen(
            [str(exe_path)],
            cwd=str(exe_dir),
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        print(f"[capture] PID: {proc.pid}")

        # 处理全屏对话框 — 自动点击“否(N)”
        print("[capture] Handling fullscreen dialog...")
        time.sleep(1.0)
        dismissed = dismiss_fullscreen_dialog(proc.pid)
        if dismissed:
            print("[capture] Dialog dismissed (windowed mode).")
        else:
            print("[capture] No dialog found, continuing...")
        time.sleep(2.0)  # 等待游戏窗口出现

    # 等待游戏窗口出现 (对话框关闭后新窗口)
    print(f"[capture] Waiting for game window...")
    hwnd = None
    for _ in range(150):  # 最多等 15 秒
        if proc:
            hwnd = find_window_by_pid(proc.pid)
        else:
            hwnd = find_window_by_title("KF")
        if hwnd:
            # 确认不是对话框 (游戏窗口应该较大)
            rect = wt.RECT()
            user32.GetClientRect(hwnd, ctypes.byref(rect))
            if rect.right > 400 and rect.bottom > 300:
                break
            hwnd = None
        time.sleep(0.1)

    if not hwnd:
        print("[ERROR] 未找到游戏窗口！", file=sys.stderr)
        if proc:
            proc.terminate()
        return 1

    rect = wt.RECT()
    user32.GetClientRect(hwnd, ctypes.byref(rect))
    print(f"[capture] Found game window: HWND={hwnd}, size={rect.right}x{rect.bottom}")

    # 确保窗口在前台
    bring_to_front(hwnd)

    # 等待画面加载
    wait_time = args.wait
    if args.result:
        print(f"[capture] 等待 Title 加载 ({wait_time}s)...")
        time.sleep(wait_time)
        # 先截 Title
        out_title = out_dir / "kf_original_title.png"
        w, h, rows = capture_window(hwnd)
        write_png(out_title, w, h, rows)

        print()
        print("[capture] === 请手动操作 KF exe 进入 Result 画面 ===")
        print("[capture] 准备好后按 Enter 截图...")
        input()
        out_result = out_dir / "kf_original_result.png"
        w, h, rows = capture_window(hwnd)
        write_png(out_result, w, h, rows)
    else:
        print(f"[capture] Waiting {wait_time}s for title screen to load...")
        time.sleep(wait_time)

        # 截图
        out_name = args.out_name or "kf_original_title.png"
        out_path = out_dir / out_name
        w, h, rows = capture_window(hwnd)
        write_png(out_path, w, h, rows)

    # 关闭 exe (如果是我们启动的)
    if proc:
        print("[capture] Terminating exe...")
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()

    print("[capture] Done.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
