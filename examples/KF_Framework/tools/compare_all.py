#!/usr/bin/env python3
"""
compare_all.py — 三后端跨平台全面对比工具

功能:
  1. 自动截取缺失的后端截图 (Vulkan/OpenGL/DX11)
  2. 各后端 vs KF 原版对比 (RMSE / 亮度 / 误差分布)
  3. 跨后端一致性对比 (GL↔VK / DX11↔GL 等)
  4. 区域诊断: 城堡/桥梁/地面/角色 分区暗像素分析
  5. 生成 heatmap 差异图

用法:
    python tools/compare_all.py                     # 对比已有截图
    python tools/compare_all.py --auto-capture      # 自动截取缺失截图再对比
    python tools/compare_all.py --capture-only       # 仅截取所有后端截图
    python tools/compare_all.py --backends vulkan opengl  # 仅指定后端
    python tools/compare_all.py --frames 300         # DSE 运行帧数
"""

import argparse
import os
import subprocess
import sys
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
    HAS_PIL = True
except ImportError:
    HAS_PIL = False
    print("[WARN] PIL/numpy not found, some features disabled")

# ============================================================================
#  路径配置
# ============================================================================

SCRIPT_DIR = Path(__file__).resolve().parent
KF_DIR = SCRIPT_DIR.parent
ENGINE_ROOT = KF_DIR.parent.parent

KF_SCREENSHOT = KF_DIR / "screenshots" / "kf_original_battle.png"
SCREENSHOT_DIR = ENGINE_ROOT / "screenshots"

BACKENDS = {
    "Vulkan":   SCREENSHOT_DIR / "vulkan.png",
    "OpenGL":   SCREENSHOT_DIR / "opengl.png",
    "DX11":     SCREENSHOT_DIR / "dx11.png",
}

# RHI 后端名称映射 (用于 DSE_RHI_BACKEND 环境变量)
BACKEND_RHI_NAME = {
    "Vulkan": "vulkan",
    "OpenGL": "opengl",
    "DX11":   "dx11",
}

# 感兴趣区域 (ROI): 名称 → (y1, y2, x1, x2) — 基于 1024x576 或 1280x720 分辨率
# 坐标会按实际分辨率自动缩放
REGIONS_720P = {
    "castle":     (100, 280, 650, 920),   # 城堡墙壁+桥梁
    "bridge":     (230, 270, 700, 860),   # 桥面表面
    "right_wall": (120, 270, 870, 920),   # 城堡右侧墙面 (之前的暗块区域)
    "ground":     (350, 450, 200, 600),   # 地面
    "character":  (250, 420, 380, 550),   # 角色区域
}

# ============================================================================
#  自动截图
# ============================================================================

def find_dse_exe():
    """查找 DSEngine 可执行文件"""
    for name in ["DSEngine_Game_release.exe", "DSEngine_Game_debug.exe"]:
        p = ENGINE_ROOT / "bin" / name
        if p.exists():
            return p
    return None

def capture_backend(backend_name, out_path, frames=300, timeout=120):
    """自动截取指定后端的截图"""
    exe = find_dse_exe()
    if not exe:
        print(f"  [SKIP] DSEngine exe not found")
        return False

    rhi = BACKEND_RHI_NAME.get(backend_name, backend_name.lower())
    lua = KF_DIR / "script" / "main.lua"

    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = rhi
    env["DSE_MAX_FRAMES"] = str(frames + 50)
    env["DSE_SCREENSHOT_FRAME"] = str(frames)
    env["DSE_SCREENSHOT_PATH"] = str(out_path)
    env["DSE_SCREENSHOT_TARGET"] = "main"
    env["DSE_AUTO_BATTLE"] = "1"
    env["DSE_DISABLE_STARTUP_SCENE_REGRESSION"] = "1"
    env.pop("DSE_DATA_ROOT", None)  # 让 Lua set_data_root 生效

    print(f"  Capturing {backend_name} ({rhi})... ", end="", flush=True)
    try:
        proc = subprocess.run(
            [str(exe), f"--script={lua}", f"--rhi={rhi}"],
            cwd=str(ENGINE_ROOT), env=env,
            capture_output=True, text=True, errors="replace", timeout=timeout
        )
        if out_path.exists():
            print(f"OK ({out_path.stat().st_size} bytes)")
            return True
        else:
            print("FAILED (no output)")
            return False
    except subprocess.TimeoutExpired:
        print(f"TIMEOUT ({timeout}s)")
        return False
    except Exception as e:
        print(f"ERROR: {e}")
        return False

def auto_capture_missing(backends=None, frames=300, force=False):
    """自动截取缺失的后端截图"""
    targets = backends or list(BACKENDS.keys())
    captured = 0
    for name in targets:
        path = BACKENDS[name]
        if path.exists() and not force:
            continue
        path.parent.mkdir(parents=True, exist_ok=True)
        if capture_backend(name, path, frames):
            captured += 1
    return captured

# ============================================================================
#  对比函数
# ============================================================================

def load_image(path, target_size=(1280, 720)):
    """加载图片为 float numpy 数组, 统一到目标尺寸"""
    img = Image.open(str(path)).convert("RGB")
    if img.size != target_size:
        img = img.resize(target_size, Image.LANCZOS)
    return np.array(img, dtype=float)

def scale_region(region, actual_h, actual_w, ref_h=720, ref_w=1280):
    """将 ROI 坐标从参考分辨率缩放到实际分辨率"""
    y1, y2, x1, x2 = region
    sy, sx = actual_h / ref_h, actual_w / ref_w
    return int(y1*sy), int(y2*sy), int(x1*sx), int(x2*sx)

def compare_pair(name, a, b):
    """计算两张图的 RMSE 和亮度统计, 返回 dict"""
    diff = a - b
    rmse = np.sqrt(np.mean(diff ** 2))
    ma, mb = np.mean(a), np.mean(b)
    p5 = np.mean(np.abs(diff) < 5) * 100
    p20 = np.mean(np.abs(diff) < 20) * 100
    p80 = np.mean(np.abs(diff) < 80) * 100
    return {
        "name": name, "rmse": rmse,
        "bright_a": ma, "bright_b": mb, "diff": ma - mb,
        "p5": p5, "p20": p20, "p80": p80,
    }

def region_analysis(img, h, w):
    """分区域统计亮度和暗像素"""
    results = {}
    brightness = img.mean(axis=2)
    for rname, roi in REGIONS_720P.items():
        y1, y2, x1, x2 = scale_region(roi, h, w)
        crop = brightness[y1:y2, x1:x2]
        if crop.size == 0:
            continue
        results[rname] = {
            "mean_brightness": crop.mean(),
            "dark_px_count": int((crop < 40).sum()),
            "dark_px_pct": (crop < 40).sum() / crop.size * 100,
            "total_px": crop.size,
        }
    return results

def cross_region_compare(img_a, img_b, h, w):
    """分区域跨后端对比"""
    results = {}
    for rname, roi in REGIONS_720P.items():
        y1, y2, x1, x2 = scale_region(roi, h, w)
        ca = img_a[y1:y2, x1:x2]
        cb = img_b[y1:y2, x1:x2]
        if ca.size == 0:
            continue
        diff = ca - cb
        rmse = np.sqrt(np.mean(diff ** 2))
        ba = ca.mean(axis=2)
        bb = cb.mean(axis=2)
        # 暗像素差异
        dark_a = int((ba < 40).sum())
        dark_b = int((bb < 40).sum())
        results[rname] = {
            "rmse": rmse,
            "bright_diff": ba.mean() - bb.mean(),
            "dark_a": dark_a,
            "dark_b": dark_b,
        }
    return results

# ============================================================================
#  报告输出
# ============================================================================

def print_section(title):
    print(f"\n{'='*70}")
    print(f"  {title}")
    print(f"{'='*70}")

def print_global_table(results):
    """打印全局对比表"""
    header = f"{'Pair':<25} {'RMSE':>6} {'Bright_A':>9} {'Bright_B':>9} {'Diff':>6} {'<5':>6} {'<20':>6} {'<80':>6}"
    print(header)
    print("-" * len(header))
    for r in results:
        print(f"{r['name']:<25} {r['rmse']:6.1f} {r['bright_a']:9.1f} {r['bright_b']:9.1f} "
              f"{r['diff']:+6.1f} {r['p5']:5.1f}% {r['p20']:5.1f}% {r['p80']:5.1f}%")

def print_region_table(all_regions):
    """打印各后端区域亮度/暗像素表"""
    all_rnames = sorted({r for d in all_regions.values() for r in d})
    header = f"{'Region':<14}" + "".join(f" {'Bright':>7} {'Dark%':>6}" for _ in all_regions)
    bnames = list(all_regions.keys())
    label_line = f"{'':14}" + "".join(f" {n:>14}" for n in bnames)
    print(label_line)
    print(header)
    print("-" * len(header))
    for rname in all_rnames:
        row = f"{rname:<14}"
        for bname in bnames:
            rd = all_regions[bname].get(rname)
            if rd:
                row += f" {rd['mean_brightness']:7.1f} {rd['dark_px_pct']:5.1f}%"
            else:
                row += f" {'N/A':>7} {'N/A':>6}"
        print(row)

def print_cross_region_table(pairs_data):
    """打印跨后端区域对比表"""
    header = f"{'Region':<14} {'RMSE':>6} {'BrightΔ':>8} {'DarkA':>6} {'DarkB':>6} {'Pair':<20}"
    print(header)
    print("-" * len(header))
    for pair_name, regions in pairs_data.items():
        for rname, rd in sorted(regions.items()):
            print(f"{rname:<14} {rd['rmse']:6.1f} {rd['bright_diff']:+8.1f} "
                  f"{rd['dark_a']:6d} {rd['dark_b']:6d} {pair_name:<20}")

def diagnose_issues(imgs, kf, all_regions, cross_data):
    """自动诊断潜在问题"""
    issues = []
    # 检查各后端 vs KF
    for name, img in imgs.items():
        diff = img - kf
        rmse = np.sqrt(np.mean(diff ** 2))
        luma_diff = img.mean() - kf.mean()
        if rmse > 60:
            issues.append(f"⚠ {name} vs KF: HIGH RMSE={rmse:.1f} — 整体差异大")
        if abs(luma_diff) > 20:
            issues.append(f"⚠ {name} vs KF: 亮度差={luma_diff:+.1f} — 检查 gamma/tone mapping")

    # 检查区域暗像素异常
    for bname, regions in all_regions.items():
        for rname, rd in regions.items():
            if rd["dark_px_pct"] > 20 and rname in ("bridge", "ground"):
                issues.append(f"⚠ {bname}/{rname}: 暗像素={rd['dark_px_pct']:.0f}% — 可能有阴影/法线/剔除问题")

    # 检查跨后端区域一致性
    for pair_name, regions in cross_data.items():
        for rname, rd in regions.items():
            if rd["rmse"] > 40 and rname in ("castle", "bridge", "right_wall"):
                issues.append(f"⚠ {pair_name}/{rname}: RMSE={rd['rmse']:.1f} — 后端不一致")
            dark_diff = abs(rd["dark_a"] - rd["dark_b"])
            total = max(rd["dark_a"], rd["dark_b"], 1)
            if dark_diff > 500 and dark_diff / total > 0.5:
                issues.append(f"⚠ {pair_name}/{rname}: 暗像素差={dark_diff} — 面剔除/绕序问题?")

    return issues

def generate_heatmaps(imgs, kf, out_dir):
    """生成差异 heatmap"""
    for name, img in imgs.items():
        diff = np.abs(img - kf)
        mx = np.max(diff, axis=2)
        h = np.clip(mx * 3, 0, 255).astype(np.uint8)
        r = h
        g = np.clip(255 - h, 0, 255).astype(np.uint8)
        bl = np.zeros_like(h)
        hm = np.stack([r, g, bl], axis=2)
        out = out_dir / f"heatmap_{name.lower()}_vs_kf.png"
        Image.fromarray(hm).save(str(out))
        print(f"  Heatmap: {out}")

# ============================================================================
#  Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="三后端跨平台全面对比工具")
    parser.add_argument("--auto-capture", action="store_true",
                        help="自动截取缺失的后端截图")
    parser.add_argument("--capture-only", action="store_true",
                        help="仅截取截图，不对比")
    parser.add_argument("--force-capture", action="store_true",
                        help="强制重新截取所有截图")
    parser.add_argument("--backends", nargs="+", default=None,
                        help="指定后端 (默认全部): Vulkan OpenGL DX11")
    parser.add_argument("--frames", type=int, default=300,
                        help="DSE 运行帧数 (默认 300)")
    parser.add_argument("--no-heatmap", action="store_true",
                        help="不生成 heatmap")
    args = parser.parse_args()

    if not HAS_PIL:
        print("[ERROR] PIL and numpy are required. Install: pip install Pillow numpy")
        return 1

    # --- 自动截图 ---
    if args.auto_capture or args.capture_only or args.force_capture:
        print_section("Auto Capture")
        backends = args.backends or list(BACKENDS.keys())
        n = auto_capture_missing(backends, args.frames, args.force_capture)
        print(f"\n  Captured {n} screenshots")
        if args.capture_only:
            return 0

    # --- 加载图片 ---
    print_section("Loading Screenshots")

    kf = None
    if KF_SCREENSHOT.exists():
        kf = load_image(KF_SCREENSHOT)
        print(f"  KF original: {KF_SCREENSHOT} ({kf.shape[1]}x{kf.shape[0]})")
    else:
        print(f"  [WARN] KF original not found: {KF_SCREENSHOT}")

    target_backends = args.backends or list(BACKENDS.keys())
    imgs = {}
    for name in target_backends:
        path = BACKENDS.get(name)
        if path and path.exists():
            imgs[name] = load_image(path)
            print(f"  {name}: {path}")
        else:
            print(f"  {name}: [MISSING] {path}")

    if not imgs:
        print("\n  [ERROR] No screenshots found! Use --auto-capture to generate them.")
        return 1

    h, w = 720, 1280  # target size after load_image

    # --- Section 1: vs KF ---
    if kf is not None:
        print_section("Section 1: Backend vs KF Original")
        results = []
        for name, img in imgs.items():
            results.append(compare_pair(f"{name} vs KF", img, kf))
        print_global_table(results)

    # --- Section 2: Cross-backend ---
    print_section("Section 2: Cross-Backend Consistency")
    cross_pairs = []
    pair_keys = [("OpenGL", "Vulkan"), ("DX11", "OpenGL"), ("DX11", "Vulkan")]
    cross_results = []
    for a, b in pair_keys:
        if a in imgs and b in imgs:
            cross_results.append(compare_pair(f"{a} vs {b}", imgs[a], imgs[b]))
    if cross_results:
        print_global_table(cross_results)
    else:
        print("  (需要至少两个后端截图)")

    # --- Section 3: 区域亮度/暗像素分析 ---
    print_section("Section 3: Region Brightness & Dark Pixel Analysis")
    all_regions = {}
    for name, img in imgs.items():
        all_regions[name] = region_analysis(img, h, w)
    if kf is not None:
        all_regions["KF"] = region_analysis(kf, h, w)
    print_region_table(all_regions)

    # --- Section 4: 跨后端区域对比 ---
    print_section("Section 4: Cross-Backend Region Comparison")
    cross_region_data = {}
    priority_pairs = [("OpenGL", "Vulkan"), ("DX11", "OpenGL"), ("DX11", "Vulkan")]
    for a, b in priority_pairs:
        if a in imgs and b in imgs:
            pair_name = f"{a}↔{b}"
            cross_region_data[pair_name] = cross_region_compare(imgs[a], imgs[b], h, w)
    if cross_region_data:
        print_cross_region_table(cross_region_data)
    else:
        print("  (需要至少两个后端截图)")

    # --- Section 5: 自动诊断 ---
    print_section("Section 5: Diagnostics")
    issues = diagnose_issues(imgs, kf, all_regions, cross_region_data) if kf is not None else []
    if issues:
        for issue in issues:
            print(f"  {issue}")
    else:
        print("  ✓ No issues detected — all backends look consistent")

    # --- Heatmaps ---
    if not args.no_heatmap and kf is not None:
        print_section("Heatmaps")
        SCREENSHOT_DIR.mkdir(parents=True, exist_ok=True)
        generate_heatmaps(imgs, kf, SCREENSHOT_DIR)

    # --- 保存报告 ---
    report_path = KF_DIR / "screenshots" / "compare_all_report.txt"
    report_path.parent.mkdir(parents=True, exist_ok=True)
    # 简单保存到文件 (重定向 stdout 太复杂, 直接写关键数据)
    with open(str(report_path), "w", encoding="utf-8") as f:
        f.write("compare_all.py report\n")
        f.write("=" * 60 + "\n")
        if kf is not None:
            for name, img in imgs.items():
                r = compare_pair(f"{name} vs KF", img, kf)
                f.write(f"{r['name']}: RMSE={r['rmse']:.1f} bright={r['bright_a']:.1f} "
                        f"diff={r['diff']:+.1f}\n")
        f.write("\nRegion analysis:\n")
        for bname, regions in all_regions.items():
            for rname, rd in sorted(regions.items()):
                f.write(f"  {bname}/{rname}: bright={rd['mean_brightness']:.1f} "
                        f"dark={rd['dark_px_pct']:.1f}%\n")
        if issues:
            f.write("\nIssues:\n")
            for issue in issues:
                f.write(f"  {issue}\n")
        else:
            f.write("\nNo issues detected.\n")
    print(f"\n  Report saved: {report_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
