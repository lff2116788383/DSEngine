#!/usr/bin/env python3
"""HD-2D screenshot pixel evidence helper.

Usage:
  python hd2d_pixel_stats.py shot1.png [shot2.png ...]
  python hd2d_pixel_stats.py --gate shot1.png shot2.png   # 带阈值门控（CI 用）

Prints per-image mean RGB/luma, bright-pixel ratio, warm-emissive ratio, and
pairwise PSNR plus global SSIM (a lightweight no-dependency evidence gate).

默认只打印证据、不判定；加 --gate 后按保守阈值判定，任一项不达标即 exit=1，
供 CI 把「渲染出非空画面且三后端一致」变成真正的门。
"""
import argparse
import math
import sys
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
except Exception as exc:  # pragma: no cover - tooling guard
    print(f"missing dependency: {exc}", file=sys.stderr)
    sys.exit(2)

# --gate 使用的保守阈值：按 M6 验收场景标定（本机 RTX 3070 实测 mean_luma≈66 /
# bright_ratio≈0.20 / 跨后端 PSNR 最低≈12dB）。M4/M5 相机与图集用例是暗场夜景，
# bright_ratio 为 0，不能套用本门控——那些用例只做「exit=0 + 出图」检查。
# 门控只用于拦住「黑屏 / 空场景 / 后端画面完全对不上」这类真实回归，不做像素级 golden
# 对比（跨 GPU 会误报）。PSNR 门限特意留出余量：Vulkan 与 D3D11 的色调映射/精度差异
# 实测已到 ~12dB，取 8dB 可避免驱动小改动即翻红。
GATE_MIN_MEAN_LUMA = 12.0
GATE_MIN_BRIGHT_RATIO = 0.0005
GATE_MIN_PAIR_PSNR = 8.0


def load_rgb(path: Path) -> np.ndarray:
    return np.asarray(Image.open(path).convert("RGB"), dtype=np.float64) / 255.0


def stats(path: Path, img: np.ndarray) -> dict:
    luma = 0.2126 * img[:, :, 0] + 0.7152 * img[:, :, 1] + 0.0722 * img[:, :, 2]
    bright = np.max(img, axis=2) > (200.0 / 255.0)
    warm = (img[:, :, 0] > 0.70) & (img[:, :, 0] > img[:, :, 1] + 0.16) & (img[:, :, 0] > img[:, :, 2] + 0.16)
    return {
        "path": str(path),
        "size": f"{img.shape[1]}x{img.shape[0]}",
        "mean_rgb": tuple(float(x) * 255.0 for x in img.reshape(-1, 3).mean(axis=0)),
        "mean_luma": float(luma.mean()) * 255.0,
        "bright_ratio": float(bright.mean()),
        "warm_emissive_ratio": float(warm.mean()),
    }


def global_ssim(a: np.ndarray, b: np.ndarray) -> float:
    x = a.reshape(-1)
    y = b.reshape(-1)
    mx, my = x.mean(), y.mean()
    vx, vy = x.var(), y.var()
    cov = ((x - mx) * (y - my)).mean()
    c1 = (0.01 ** 2)
    c2 = (0.03 ** 2)
    return float(((2 * mx * my + c1) * (2 * cov + c2)) /
                 ((mx * mx + my * my + c1) * (vx + vy + c2) + 1e-12))


def psnr(a: np.ndarray, b: np.ndarray) -> float:
    mse = float(np.mean((a - b) ** 2))
    if mse <= 1e-12:
        return 99.0
    return 10.0 * math.log10(1.0 / mse)


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(description="HD-2D screenshot pixel evidence")
    parser.add_argument("images", nargs="*", help="screenshot PNG 路径")
    parser.add_argument("--gate", action="store_true",
                        help="启用保守阈值门控（不达标 exit=1）")
    parser.add_argument("--min-mean-luma", type=float, default=GATE_MIN_MEAN_LUMA)
    parser.add_argument("--min-bright-ratio", type=float, default=GATE_MIN_BRIGHT_RATIO)
    parser.add_argument("--min-pair-psnr", type=float, default=GATE_MIN_PAIR_PSNR)
    args = parser.parse_args(argv[1:])

    if not args.images:
        print(__doc__)
        return 1
    paths = [Path(p) for p in args.images]
    images = []
    for p in paths:
        if not p.exists():
            print(f"missing image: {p}", file=sys.stderr)
            return 1
        images.append(load_rgb(p))

    failures: list[str] = []
    for p, img in zip(paths, images):
        s = stats(p, img)
        rgb = ", ".join(f"{v:.2f}" for v in s["mean_rgb"])
        print(f"[pixel] {s['path']} size={s['size']} mean_rgb=({rgb}) "
              f"mean_luma={s['mean_luma']:.2f} bright_ratio={s['bright_ratio']:.5f} "
              f"warm_emissive_ratio={s['warm_emissive_ratio']:.5f}")
        if args.gate:
            if s["mean_luma"] < args.min_mean_luma:
                failures.append(f"{p.name}: mean_luma {s['mean_luma']:.2f} < {args.min_mean_luma}")
            if s["bright_ratio"] < args.min_bright_ratio:
                failures.append(
                    f"{p.name}: bright_ratio {s['bright_ratio']:.5f} < {args.min_bright_ratio}")

    for i in range(len(images)):
        for j in range(i + 1, len(images)):
            if images[i].shape != images[j].shape:
                print(f"[pair] {paths[i].name} != {paths[j].name}: size mismatch")
                failures.append(f"{paths[i].name} vs {paths[j].name}: 尺寸不一致")
                continue
            pair_psnr = psnr(images[i], images[j])
            print(f"[pair] {paths[i].name} vs {paths[j].name}: "
                  f"PSNR={pair_psnr:.2f}dB SSIM={global_ssim(images[i], images[j]):.4f}")
            if args.gate and pair_psnr < args.min_pair_psnr:
                failures.append(
                    f"{paths[i].name} vs {paths[j].name}: PSNR {pair_psnr:.2f}dB < "
                    f"{args.min_pair_psnr}")

    if failures:
        for f in failures:
            print(f"[gate] FAIL {f}", file=sys.stderr)
        return 1
    if args.gate:
        print("[gate] PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
