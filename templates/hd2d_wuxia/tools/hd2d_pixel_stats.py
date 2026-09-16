#!/usr/bin/env python3
"""HD-2D screenshot pixel evidence helper.

Usage:
  python hd2d_pixel_stats.py shot1.png [shot2.png ...]

Prints per-image mean RGB/luma, bright-pixel ratio, warm-emissive ratio, and
pairwise PSNR plus global SSIM (a lightweight no-dependency evidence gate).
"""
import math
import sys
from pathlib import Path

try:
    from PIL import Image
    import numpy as np
except Exception as exc:  # pragma: no cover - tooling guard
    print(f"missing dependency: {exc}", file=sys.stderr)
    sys.exit(2)


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
    if len(argv) < 2:
        print(__doc__)
        return 1
    paths = [Path(p) for p in argv[1:]]
    images = []
    for p in paths:
        if not p.exists():
            print(f"missing image: {p}", file=sys.stderr)
            return 1
        images.append(load_rgb(p))

    for p, img in zip(paths, images):
        s = stats(p, img)
        rgb = ", ".join(f"{v:.2f}" for v in s["mean_rgb"])
        print(f"[pixel] {s['path']} size={s['size']} mean_rgb=({rgb}) "
              f"mean_luma={s['mean_luma']:.2f} bright_ratio={s['bright_ratio']:.5f} "
              f"warm_emissive_ratio={s['warm_emissive_ratio']:.5f}")

    for i in range(len(images)):
        for j in range(i + 1, len(images)):
            if images[i].shape != images[j].shape:
                print(f"[pair] {paths[i].name} != {paths[j].name}: size mismatch")
                continue
            print(f"[pair] {paths[i].name} vs {paths[j].name}: "
                  f"PSNR={psnr(images[i], images[j]):.2f}dB SSIM={global_ssim(images[i], images[j]):.4f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
