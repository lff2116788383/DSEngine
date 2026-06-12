#!/usr/bin/env python3
"""
run_benchmark.py — 自动化压测: 多后端 × GPU-Driven ON/OFF × 不同实体数量
用法:
    python examples/stress_test/run_benchmark.py
    python examples/stress_test/run_benchmark.py --counts 100 500 2000
    python examples/stress_test/run_benchmark.py --backends opengl dx11
    python examples/stress_test/run_benchmark.py --no-gpu-driven
"""

import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ENGINE_ROOT = SCRIPT_DIR.parent.parent

def find_exe():
    candidates = [
        ENGINE_ROOT / "bin" / "RelWithDebInfo" / "DSEngine_Game_relwithdebinfo.exe",
        ENGINE_ROOT / "bin" / "Release" / "DSEngine_Game_release.exe",
        ENGINE_ROOT / "bin" / "DSEngine_Game_release.exe",
        ENGINE_ROOT / "bin" / "DSEngine_Game_debug.exe",
    ]
    for p in candidates:
        if p.exists():
            return p
    return None

def run_single(exe, backend, gpu_driven, entity_count, perf_frames=600, timeout=300):
    """运行单次压测, 返回解析后的性能数据 dict 或 None"""
    env = os.environ.copy()
    env["DSE_RHI_BACKEND"] = backend
    env["DSE_DISABLE_GPU_DRIVEN"] = "0" if gpu_driven else "1"
    env["DSE_ENTITY_COUNT"] = str(entity_count)
    env["DSE_PERF_FRAMES"] = str(perf_frames)
    env["DSE_ANIM_ENABLED"] = "1"
    env["DSE_DATA_ROOT"] = "examples/KF_Framework"
    env["DSE_DISABLE_STARTUP_SCENE_REGRESSION"] = "1"
    total_frames = 60 + perf_frames + 30  # warmup + sample + buffer
    env["DSE_MAX_FRAMES"] = str(total_frames)

    lua = SCRIPT_DIR / "script" / "main.lua"
    label = f"{backend}/{'gpu' if gpu_driven else 'cpu'}/{entity_count}"
    print(f"  [{label}] Running... ", end="", flush=True)

    try:
        proc = subprocess.run(
            [str(exe), f"--script={lua}", f"--rhi={backend}"],
            cwd=str(ENGINE_ROOT), env=env,
            capture_output=True, text=True, errors="replace", timeout=timeout
        )
        output = proc.stdout + proc.stderr

        # 解析 DSE_PERF_RESULT 行
        m = re.search(
            r"DSE_PERF_RESULT entities=(\d+) fps_avg=([\d.]+) fps_min=([\d.]+) "
            r"ft_avg=([\d.]+) ft_p99=([\d.]+)",
            output
        )
        # draw_calls / load_ms 为后加字段，单独可选解析（兼容旧输出）。
        dc_m = re.search(r"draw_calls=(\d+)", output)
        load_m = re.search(r"load_ms=([\d.]+)", output)
        draw_calls = int(dc_m.group(1)) if dc_m else 0
        load_ms = float(load_m.group(1)) if load_m else 0.0
        # 解析 DSE_RENDER_DEVICE 行（实际渲染设备 + 软渲标志），用于区分硬件/软渲。
        dev = re.search(r'DSE_RENDER_DEVICE backend=\S+ adapter="([^"]*)" software=(\d)', output)
        adapter = dev.group(1) if dev else "unknown"
        software = (dev.group(2) == "1") if dev else False
        if m:
            result = {
                "backend": backend,
                "gpu_driven": gpu_driven,
                "entities": int(m.group(1)),
                "fps_avg": float(m.group(2)),
                "fps_min": float(m.group(3)),
                "ft_avg": float(m.group(4)),
                "ft_p99": float(m.group(5)),
                "draw_calls": draw_calls,
                "load_ms": load_ms,
                "adapter": adapter,
                "software": software,
            }
            sw_tag = "  [SOFTWARE]" if software else ""
            print(f"fps_avg={result['fps_avg']:.1f} fps_min={result['fps_min']:.1f} "
                  f"dc={draw_calls} load={load_ms:.0f}ms dev={adapter}{sw_tag}")
            return result
        else:
            print("FAILED (no perf result)")
            # 输出最后几行帮助调试
            lines = output.strip().split("\n")
            for line in lines[-5:]:
                print(f"    > {line}")
            return None
    except subprocess.TimeoutExpired:
        print(f"TIMEOUT ({timeout}s)")
        return None
    except Exception as e:
        print(f"ERROR: {e}")
        return None

def print_report(results):
    """打印对比表格"""
    if not results:
        print("\nNo results collected.")
        return

    print("\n" + "=" * 80)
    print("  BENCHMARK RESULTS")
    print("=" * 80)
    header = (f"{'Backend':<10} {'Mode':<6} {'Entities':>8} {'FPS avg':>8} {'FPS min':>8} "
              f"{'FT avg':>8} {'FT p99':>8} {'Draws':>7} {'Load ms':>8} {'SW':>3} {'Device':<24}")
    print(header)
    print("-" * len(header))
    for r in results:
        mode = "GPU" if r["gpu_driven"] else "CPU"
        sw = "Y" if r.get("software") else ""
        print(f"{r['backend']:<10} {mode:<6} {r['entities']:>8} "
              f"{r['fps_avg']:>8.1f} {r['fps_min']:>8.1f} "
              f"{r['ft_avg']:>8.2f} {r['ft_p99']:>8.2f} "
              f"{r.get('draw_calls', 0):>7} {r.get('load_ms', 0.0):>8.1f} "
              f"{sw:>3} {r.get('adapter', 'unknown'):<24}")

    if any(r.get("software") for r in results):
        print("\n  [!] Rows marked SW=Y ran on a software rasterizer "
              "(WARP / Basic Render Driver / llvmpipe, etc.).")
        print("      These are NOT hardware numbers and are not comparable to "
              "hardware results; re-run on a machine with a discrete GPU.")

    # GPU-driven 加速比
    print("\n" + "-" * 80)
    print("  GPU-Driven Speedup (FPS avg ratio: GPU/CPU)")
    print("-" * 80)
    cpu_map = {}
    gpu_map = {}
    for r in results:
        key = (r["backend"], r["entities"])
        if r["gpu_driven"]:
            gpu_map[key] = r
        else:
            cpu_map[key] = r
    for key in sorted(cpu_map.keys()):
        if key in gpu_map:
            speedup = gpu_map[key]["fps_avg"] / max(cpu_map[key]["fps_avg"], 0.1)
            print(f"  {key[0]:<10} {key[1]:>6} entities: {speedup:.2f}x "
                  f"({cpu_map[key]['fps_avg']:.1f} → {gpu_map[key]['fps_avg']:.1f} fps)")
    print("=" * 80)

def main():
    parser = argparse.ArgumentParser(description="DSEngine 性能压测")
    parser.add_argument("--backends", nargs="+", default=["opengl", "dx11", "vulkan"])
    parser.add_argument("--counts", nargs="+", type=int, default=[100, 500, 2000])
    parser.add_argument("--no-gpu-driven", action="store_true", help="仅测 CPU 模式")
    parser.add_argument("--gpu-only", action="store_true", help="仅测 GPU-driven 模式")
    parser.add_argument("--frames", type=int, default=600, help="采样帧数")
    args = parser.parse_args()

    exe = find_exe()
    if not exe:
        print("[ERROR] DSEngine exe not found")
        return 1

    print(f"[Benchmark] exe: {exe}")
    print(f"[Benchmark] backends: {args.backends}")
    print(f"[Benchmark] counts: {args.counts}")
    print(f"[Benchmark] frames: {args.frames}")

    modes = []
    if not args.gpu_only:
        modes.append(False)  # CPU
    if not args.no_gpu_driven:
        modes.append(True)   # GPU

    results = []
    for count in args.counts:
        for backend in args.backends:
            for gpu_driven in modes:
                r = run_single(exe, backend, gpu_driven, count, args.frames)
                if r:
                    results.append(r)

    print_report(results)

    # 保存 CSV
    csv_path = SCRIPT_DIR / "benchmark_results.csv"
    with open(str(csv_path), "w") as f:
        f.write("backend,gpu_driven,entities,fps_avg,fps_min,ft_avg_ms,ft_p99_ms,"
                "draw_calls,load_ms,adapter,software\n")
        for r in results:
            adapter = r.get("adapter", "unknown").replace(",", " ")
            f.write(f"{r['backend']},{r['gpu_driven']},{r['entities']},"
                    f"{r['fps_avg']:.1f},{r['fps_min']:.1f},"
                    f"{r['ft_avg']:.2f},{r['ft_p99']:.2f},"
                    f"{r.get('draw_calls', 0)},{r.get('load_ms', 0.0):.1f},"
                    f"{adapter},{r.get('software', False)}\n")
    print(f"\nCSV saved: {csv_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
