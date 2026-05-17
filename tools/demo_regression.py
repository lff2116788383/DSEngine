"""Demo 自动截图回归测试脚本 (OpenGL / DX11 / Vulkan)

用法:
    python tools/demo_regression.py --baseline                      # OpenGL 基线
    python tools/demo_regression.py --baseline --backend=dx11       # DX11 基线
    python tools/demo_regression.py --baseline --backend=all        # 三后端基线
    python tools/demo_regression.py --compare                       # OpenGL 回归
    python tools/demo_regression.py --compare --backend=all         # 三后端回归
    python tools/demo_regression.py --cross-compare                 # 跨后端对比报告
    python tools/demo_regression.py --cross-compare --threshold=20

依赖: Python 3.9+ / Pillow (pip install Pillow)
"""

import argparse
import itertools
import math
import os
import pathlib
import shutil
import subprocess
import sys
import time
from typing import Optional

# Windows stdout 编码修复
if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")
    for stream in (sys.stdout, sys.stderr):
        if stream and hasattr(stream, "reconfigure"):
            try:
                stream.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass

# ---------------------------------------------------------------------------
# 59 个 3D demo 列表（来自 samples/lua/3d/README.md）
# ---------------------------------------------------------------------------
ALL_DEMOS = [
    # 入门 / 基础图元
    "3d_triangle",
    "3d_square",
    "3d_cube",
    # 渲染 / 材质
    "3d_static_model",
    "3d_textured_cube",
    "3d_material_showcase",
    "3d_advanced_pbr_showcase",
    "3d_texture_material_slots",
    "3d_lighting_showcase",
    "3d_shadow_showcase",
    "3d_skybox_environment",
    "3d_postprocess_showcase",
    "3d_render_quality_showcase",
    # 动画
    "3d_animation_basic",
    "3d_animation_ik_layers",
    "3d_character_third_person",
    "3d_character_controller",
    # 物理
    "3d_physics_stack",
    "3d_physics_interaction",
    "3d_physics_raycast_pick",
    "3d_physics_triggers",
    "3d_fracture",
    "3d_cloth",
    "3d_ragdoll",
    "3d_softbody",
    "3d_buoyancy",
    "3d_vehicle",
    "3d_rope",
    "3d_fluid",
    # 音频
    "3d_audio_spatial",
    "3d_audio_complete",
    # 地形 / 场景
    "3d_terrain_heightmap",
    "3d_terrain_lod_zones",
    "3d_scene_showcase",
    "3d_scene_load",
    "3d_asset_pack_showcase",
    # UI / 输入 / 调试
    "3d_camera_showcase",
    "3d_input_showcase",
    "3d_hud_overlay",
    "3d_metrics_debug",
    # 程序化生成
    "3d_procedural_mesh",
    "3d_particles_showcase",
    # GPU / 管线验证
    "3d_instancing",
    "3d_gpu_culling",
    "3d_streaming_load",
    "3d_compute_basic",
    "3d_transparency",
    "3d_navmesh",
    # 环境 / 特效
    "3d_water",
    "3d_decal",
    "3d_fog_atmosphere",
    # 高级渲染
    "3d_lod",
    "3d_morph_target",
    "3d_reflection_probe",
    "3d_gi_probe",
    "3d_hair",
    "3d_light_probe",
    "3d_postprocess_effects",
    # AI / 行为
    "3d_steering_behavior",
]

# ---------------------------------------------------------------------------
# 常量
# ---------------------------------------------------------------------------
DEFAULT_MAX_FRAMES = 30
DEFAULT_SCREENSHOT_FRAME = 25
DEFAULT_TIMEOUT = 60  # seconds per demo
DEFAULT_THRESHOLD = 5.0
CROSS_COMPARE_THRESHOLD = 20.0  # 跨后端容差更大
VALID_BACKENDS = ["opengl", "dx11", "vulkan"]
BASELINE_DIR_BASE = pathlib.Path("tests") / "regression" / "screenshots"


def baseline_dir_for(root: pathlib.Path, backend: str) -> pathlib.Path:
    return root / BASELINE_DIR_BASE / backend


def find_exe(root: pathlib.Path) -> Optional[pathlib.Path]:
    """查找可用的引擎可执行文件"""
    candidates = [
        root / "bin" / "DSEngine_Game_release.exe",
        root / "bin" / "RelWithDebInfo" / "DSEngine_Game_relwithdebinfo.exe",
        root / "bin" / "DSEngine_Game_debug.exe",
        root / "bin" / "DSEngine_lua_debug.exe",
    ]
    for c in candidates:
        if c.exists():
            return c
    return None


def sync_samples(root: pathlib.Path) -> None:
    """同步 samples/ 到 bin/samples/"""
    src = root / "samples"
    dst = root / "bin" / "samples"
    if not src.exists():
        return
    print(f"SYNC {src} -> {dst}", flush=True)
    if dst.exists():
        shutil.rmtree(dst, ignore_errors=True)
    shutil.copytree(src, dst, dirs_exist_ok=True)


def run_demo(
    root: pathlib.Path,
    exe: pathlib.Path,
    demo: str,
    screenshot_path: pathlib.Path,
    max_frames: int,
    screenshot_frame: int,
    timeout: int,
    backend: str = "opengl",
) -> tuple[bool, str]:
    """运行单个 demo，返回 (成功, 信息)"""
    env = os.environ.copy()
    env["DSE_DEMO"] = demo
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_MAX_FRAMES"] = str(max_frames)
    env["DSE_SCREENSHOT_FRAME"] = str(screenshot_frame)
    env["DSE_SCREENSHOT_PATH"] = str(screenshot_path)
    env["DSE_AUTO_BATTLE"] = "1"
    env["DSE_SCREENSHOT_TARGET"] = "main"

    screenshot_path.parent.mkdir(parents=True, exist_ok=True)

    cmd = [str(exe)]
    # standalone exe (DSEngine_Game_*) 需要 --script 参数
    if "Game" in exe.name or "standalone" in exe.name:
        cmd.append("--script=samples/lua/main.lua")

    try:
        proc = subprocess.run(
            cmd,
            cwd=str(root / "bin"),
            env=env,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            errors="replace",
            timeout=timeout,
        )
        if proc.returncode != 0 and proc.returncode != -2:
            # -2 是截图失败的返回码，其他非零可能是异常退出
            stderr_snippet = (proc.stdout or "")[-500:]
            return False, f"exit_code={proc.returncode} [{stderr_snippet.strip()[-200:]}]"
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as exc:
        return False, f"EXCEPTION: {exc}"

    if not screenshot_path.exists():
        return False, "screenshot_not_generated"
    if screenshot_path.stat().st_size < 512:
        return False, f"screenshot_too_small ({screenshot_path.stat().st_size}B)"

    return True, "ok"


def compute_rmse(img_a_path: pathlib.Path, img_b_path: pathlib.Path) -> Optional[float]:
    """用 Pillow 计算两张图的 RMSE"""
    try:
        from PIL import Image
        import numpy as np
    except ImportError:
        # 无 numpy 时用纯 Python
        return _compute_rmse_pure(img_a_path, img_b_path)

    img_a = Image.open(img_a_path).convert("RGB")
    img_b = Image.open(img_b_path).convert("RGB")
    if img_a.size != img_b.size:
        # 尺寸不同，resize 较大的
        img_b = img_b.resize(img_a.size, Image.LANCZOS)

    arr_a = np.asarray(img_a, dtype=np.float64)
    arr_b = np.asarray(img_b, dtype=np.float64)
    mse = np.mean((arr_a - arr_b) ** 2)
    return math.sqrt(float(mse))


def _compute_rmse_pure(img_a_path: pathlib.Path, img_b_path: pathlib.Path) -> Optional[float]:
    """纯 Pillow（无 numpy）RMSE 计算"""
    try:
        from PIL import Image
    except ImportError:
        print("ERROR: Pillow not installed. Run: pip install Pillow", file=sys.stderr)
        return None

    img_a = Image.open(img_a_path).convert("RGB")
    img_b = Image.open(img_b_path).convert("RGB")
    if img_a.size != img_b.size:
        img_b = img_b.resize(img_a.size, Image.LANCZOS)

    pixels_a = list(img_a.getdata())
    pixels_b = list(img_b.getdata())
    total = len(pixels_a) * 3
    sum_sq = 0.0
    for (r1, g1, b1), (r2, g2, b2) in zip(pixels_a, pixels_b):
        sum_sq += (r1 - r2) ** 2 + (g1 - g2) ** 2 + (b1 - b2) ** 2
    mse = sum_sq / total
    return math.sqrt(mse)


def mode_baseline(
    root: pathlib.Path,
    exe: pathlib.Path,
    demos: list[str],
    baseline_dir: pathlib.Path,
    max_frames: int,
    screenshot_frame: int,
    timeout: int,
    backend: str = "opengl",
) -> int:
    """生成基线截图"""
    baseline_dir.mkdir(parents=True, exist_ok=True)
    passed = 0
    failed_list: list[tuple[str, str]] = []

    for i, demo in enumerate(demos, 1):
        png_path = baseline_dir / f"{demo}.png"
        print(f"[{i}/{len(demos)}] {demo} ... ", end="", flush=True)
        ok, info = run_demo(root, exe, demo, png_path, max_frames, screenshot_frame, timeout, backend)
        if ok:
            print(f"OK ({png_path.stat().st_size // 1024}KB)")
            passed += 1
        else:
            print(f"FAIL ({info})")
            failed_list.append((demo, info))

    print()
    print("=" * 60)
    print(f"BASELINE: {passed} generated, {len(failed_list)} failed")
    if failed_list:
        print("Failed demos:")
        for name, reason in failed_list:
            print(f"  - {name}: {reason}")
    print(f"Output: {baseline_dir}")
    return 0 if not failed_list else 1


def mode_compare(
    root: pathlib.Path,
    exe: pathlib.Path,
    demos: list[str],
    baseline_dir: pathlib.Path,
    threshold: float,
    max_frames: int,
    screenshot_frame: int,
    timeout: int,
    backend: str = "opengl",
) -> int:
    """与基线对比"""
    if not baseline_dir.exists():
        print(f"ERROR: 基线目录不存在: {baseline_dir}", file=sys.stderr)
        print("请先运行: python tools/demo_regression.py --baseline", file=sys.stderr)
        return 1

    tmp_dir = root / "tmp" / "regression_compare"
    tmp_dir.mkdir(parents=True, exist_ok=True)

    passed = 0
    failed_list: list[tuple[str, float, str]] = []
    skipped = 0

    for i, demo in enumerate(demos, 1):
        baseline_png = baseline_dir / f"{demo}.png"
        current_png = tmp_dir / f"{demo}.png"

        if not baseline_png.exists():
            print(f"[SKIP] {demo:<35} (no baseline)")
            skipped += 1
            continue

        # 生成当前截图
        ok, info = run_demo(root, exe, demo, current_png, max_frames, screenshot_frame, timeout, backend)
        if not ok:
            print(f"[FAIL] {demo:<35} RUN_ERROR: {info}")
            failed_list.append((demo, -1.0, f"run_error: {info}"))
            continue

        # 计算 RMSE
        rmse = compute_rmse(baseline_png, current_png)
        if rmse is None:
            print(f"[FAIL] {demo:<35} RMSE_ERROR (Pillow missing?)")
            failed_list.append((demo, -1.0, "rmse_computation_failed"))
            continue

        if rmse <= threshold:
            print(f"[PASS] {demo:<35} RMSE={rmse:.2f}")
            passed += 1
        else:
            print(f"[FAIL] {demo:<35} RMSE={rmse:.2f} (threshold={threshold:.1f})")
            failed_list.append((demo, rmse, "threshold_exceeded"))

    print()
    print("=" * 60)
    total_run = passed + len(failed_list)
    print(f"SUMMARY: {passed} pass, {len(failed_list)} fail, {skipped} skip (threshold={threshold:.1f})")
    if failed_list:
        print("Failed demos:")
        for name, rmse_val, reason in failed_list:
            if rmse_val >= 0:
                print(f"  - {name}: RMSE={rmse_val:.2f}")
            else:
                print(f"  - {name}: {reason}")
        return 1
    return 0


def generate_heatmap(
    img_a_path: pathlib.Path,
    img_b_path: pathlib.Path,
    output_path: pathlib.Path,
    label_a: str,
    label_b: str,
) -> bool:
    """生成差异热力图"""
    try:
        from PIL import Image, ImageDraw, ImageFont
        import numpy as np
    except ImportError:
        return False

    img_a = Image.open(img_a_path).convert("RGB")
    img_b = Image.open(img_b_path).convert("RGB")
    if img_a.size != img_b.size:
        img_b = img_b.resize(img_a.size, Image.LANCZOS)

    arr_a = np.asarray(img_a, dtype=np.float64)
    arr_b = np.asarray(img_b, dtype=np.float64)
    diff = np.sqrt(np.mean((arr_a - arr_b) ** 2, axis=2))  # per-pixel RMSE
    diff_norm = np.clip(diff / 80.0, 0.0, 1.0)  # normalize to [0,1]

    # 热力图：蓝(0) → 绿(0.5) → 红(1)
    h = np.zeros((*diff_norm.shape, 3), dtype=np.uint8)
    t = diff_norm
    r = np.clip(t * 2.0, 0.0, 1.0)
    g = np.clip(2.0 - t * 2.0, 0.0, 1.0) * np.clip(t * 4.0, 0.0, 1.0)
    b = np.clip(1.0 - t * 2.0, 0.0, 1.0)
    h[..., 0] = (r * 255).astype(np.uint8)
    h[..., 1] = (g * 255).astype(np.uint8)
    h[..., 2] = (b * 255).astype(np.uint8)

    heatmap_img = Image.fromarray(h)

    # 拼接：A | B | heatmap
    w, ht = img_a.size
    canvas = Image.new("RGB", (w * 3, ht + 30), (30, 30, 30))
    canvas.paste(img_a, (0, 30))
    canvas.paste(img_b, (w, 30))
    canvas.paste(heatmap_img, (w * 2, 30))

    draw = ImageDraw.Draw(canvas)
    try:
        font = ImageFont.truetype("arial.ttf", 18)
    except Exception:
        font = ImageFont.load_default()
    draw.text((10, 5), label_a, fill=(255, 255, 255), font=font)
    draw.text((w + 10, 5), label_b, fill=(255, 255, 255), font=font)
    draw.text((w * 2 + 10, 5), "Heatmap (RMSE)", fill=(255, 255, 255), font=font)

    output_path.parent.mkdir(parents=True, exist_ok=True)
    canvas.save(output_path)
    return True


def mode_cross_compare(
    root: pathlib.Path,
    demos: list[str],
    threshold: float,
) -> int:
    """三后端基线两两 RMSE 对比 + 热力图生成"""
    available = []
    for b in VALID_BACKENDS:
        d = baseline_dir_for(root, b)
        if d.exists() and any(d.glob("*.png")):
            available.append(b)

    if len(available) < 2:
        print(f"ERROR: 至少需要 2 个后端的基线截图。当前可用: {available}", file=sys.stderr)
        print("请先运行: python tools/demo_regression.py --baseline --backend=all", file=sys.stderr)
        return 1

    print(f"可用后端基线: {', '.join(available)}")
    pairs = list(itertools.combinations(available, 2))
    report_dir = root / "tests" / "regression" / "cross_compare"
    report_dir.mkdir(parents=True, exist_ok=True)

    all_results: dict[tuple[str, str], list[tuple[str, float]]] = {}
    total_fail = 0

    for ba, bb in pairs:
        pair_key = (ba, bb)
        pair_label = f"{ba} vs {bb}"
        print(f"\n{'=' * 60}")
        print(f"  跨后端对比: {pair_label}")
        print(f"{'=' * 60}")

        dir_a = baseline_dir_for(root, ba)
        dir_b = baseline_dir_for(root, bb)
        heatmap_dir = report_dir / f"{ba}_vs_{bb}"
        heatmap_dir.mkdir(parents=True, exist_ok=True)

        pair_results: list[tuple[str, float]] = []
        pair_fail = 0

        for demo in demos:
            png_a = dir_a / f"{demo}.png"
            png_b = dir_b / f"{demo}.png"

            if not png_a.exists() or not png_b.exists():
                continue

            rmse = compute_rmse(png_a, png_b)
            if rmse is None:
                continue

            status = "PASS" if rmse <= threshold else "FAIL"
            if status == "FAIL":
                pair_fail += 1
            print(f"  [{status}] {demo:<35} RMSE={rmse:.2f}")
            pair_results.append((demo, rmse))

            # 生成热力图（仅对 RMSE > 5 的 demo）
            if rmse > 5.0:
                generate_heatmap(png_a, png_b, heatmap_dir / f"{demo}.png", ba, bb)

        all_results[pair_key] = pair_results
        total_fail += pair_fail

    # 汇总报告
    print(f"\n{'=' * 60}")
    print(f"  跨后端对比汇总 (threshold={threshold:.1f})")
    print(f"{'=' * 60}")
    report_lines = []
    for (ba, bb), results in all_results.items():
        if not results:
            continue
        rmse_values = [r for _, r in results]
        avg_rmse = sum(rmse_values) / len(rmse_values)
        max_rmse = max(rmse_values)
        max_demo = [d for d, r in results if r == max_rmse][0]
        fail_count = sum(1 for _, r in results if r > threshold)
        line = f"  {ba:>8} vs {bb:<8}  avg={avg_rmse:6.2f}  max={max_rmse:6.2f} ({max_demo})  fail={fail_count}"
        print(line)
        report_lines.append(line)

    # 写入报告文件
    report_path = report_dir / "cross_compare_report.txt"
    with open(report_path, "w", encoding="utf-8") as f:
        f.write("DSEngine 跨后端视觉对比报告\n")
        f.write(f"threshold={threshold:.1f}\n\n")
        for line in report_lines:
            f.write(line + "\n")
        f.write(f"\n详细结果:\n")
        for (ba, bb), results in all_results.items():
            f.write(f"\n--- {ba} vs {bb} ---\n")
            for demo, rmse in results:
                f.write(f"  {demo:<35} RMSE={rmse:.2f}\n")

    print(f"\n  报告: {report_path}")
    print(f"  热力图: {report_dir}")
    return 1 if total_fail > 0 else 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Demo 自动截图回归测试 (OpenGL / DX11 / Vulkan)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""示例:
  python tools/demo_regression.py --baseline                      # OpenGL 基线
  python tools/demo_regression.py --baseline --backend=dx11       # DX11 基线
  python tools/demo_regression.py --baseline --backend=all        # 三后端基线
  python tools/demo_regression.py --compare                       # OpenGL 回归
  python tools/demo_regression.py --compare --backend=all         # 三后端回归
  python tools/demo_regression.py --cross-compare                 # 跨后端对比
  python tools/demo_regression.py --cross-compare --threshold=20
""",
    )
    mode_group = parser.add_mutually_exclusive_group(required=True)
    mode_group.add_argument("--baseline", action="store_true", help="生成基线截图")
    mode_group.add_argument("--compare", action="store_true", help="与基线对比")
    mode_group.add_argument("--cross-compare", action="store_true", help="跨后端基线两两 RMSE 对比 + 热力图")

    parser.add_argument("--backend", default="opengl",
                        help="渲染后端: opengl|dx11|vulkan|all (默认 opengl)")
    parser.add_argument("--demos", nargs="+", default=None, help="指定 demo 列表（默认全部 59 个）")
    parser.add_argument("--threshold", type=float, default=None,
                        help=f"RMSE 失败阈值 (baseline/compare 默认 {DEFAULT_THRESHOLD}, cross-compare 默认 {CROSS_COMPARE_THRESHOLD})")
    parser.add_argument("--max-frames", type=int, default=DEFAULT_MAX_FRAMES, help=f"每个 demo 运行帧数 (默认 {DEFAULT_MAX_FRAMES})")
    parser.add_argument("--screenshot-frame", type=int, default=DEFAULT_SCREENSHOT_FRAME, help=f"截图帧号 (默认 {DEFAULT_SCREENSHOT_FRAME})")
    parser.add_argument("--timeout", type=int, default=DEFAULT_TIMEOUT, help=f"每个 demo 超时秒数 (默认 {DEFAULT_TIMEOUT})")
    parser.add_argument("--no-sync", action="store_true", help="不同步 samples/ 到 bin/samples/")
    parser.add_argument("--baseline-dir", default=None, help="基线截图目录（覆盖默认 per-backend 目录）")

    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parent.parent

    # Demo 列表
    demos = args.demos if args.demos else ALL_DEMOS

    # 跨后端对比模式不需要 exe
    if args.cross_compare:
        threshold = args.threshold if args.threshold is not None else CROSS_COMPARE_THRESHOLD
        print(f"Root:   {root}")
        print(f"Demos:  {len(demos)}")
        return mode_cross_compare(root, demos, threshold)

    # 确定可执行文件
    exe = find_exe(root)
    if exe is None:
        print("ERROR: 未找到引擎可执行文件。请先构建:", file=sys.stderr)
        print("  build_all.bat --release", file=sys.stderr)
        return 1

    print(f"Engine: {exe}")
    print(f"Root:   {root}")

    # 同步 samples
    if not args.no_sync:
        sync_samples(root)

    print(f"Demos:  {len(demos)}")

    # 解析后端列表
    backends = VALID_BACKENDS if args.backend == "all" else [args.backend]
    for b in backends:
        if b not in VALID_BACKENDS:
            print(f"ERROR: 无效后端 '{b}'。可选: {', '.join(VALID_BACKENDS)}", file=sys.stderr)
            return 1

    threshold = args.threshold if args.threshold is not None else DEFAULT_THRESHOLD
    exit_code = 0

    for backend in backends:
        if len(backends) > 1:
            print(f"\n{'#' * 60}")
            print(f"  后端: {backend.upper()}")
            print(f"{'#' * 60}")

        bdir = pathlib.Path(args.baseline_dir) if args.baseline_dir else baseline_dir_for(root, backend)

        if args.baseline:
            rc = mode_baseline(root, exe, demos, bdir, args.max_frames, args.screenshot_frame, args.timeout, backend)
        else:
            rc = mode_compare(root, exe, demos, bdir, threshold, args.max_frames, args.screenshot_frame, args.timeout, backend)

        if rc != 0:
            exit_code = rc

    return exit_code


if __name__ == "__main__":
    raise SystemExit(main())
