#!/usr/bin/env python3
"""Minimal PNG screenshot statistics for games/wuxia_arpg (new game, no template dependency)."""
from __future__ import annotations
import argparse
from pathlib import Path
from PIL import Image, ImageStat


def luma(pixel):
    r, g, b = pixel[0], pixel[1], pixel[2]
    return 0.2126*r + 0.7152*g + 0.0722*b


def stats(path: Path):
    im = Image.open(path).convert("RGB")
    w, h = im.size
    stat = ImageStat.Stat(im)
    mean = tuple(stat.mean)
    pixels = list(im.getdata())
    n = len(pixels)
    l = sum(luma(p) for p in pixels) / n
    bright = sum(1 for p in pixels if luma(p) > 180) / n
    warm = sum(1 for p in pixels if p[0] > 150 and p[0] > p[2] * 1.25) / n
    return w, h, mean, l, bright, warm


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("images", nargs="+")
    args = ap.parse_args()
    for s in args.images:
        p = Path(s)
        w, h, mean, l, bright, warm = stats(p)
        print(f"[pixel] {p} size={w}x{h} mean_rgb=({mean[0]:.2f}, {mean[1]:.2f}, {mean[2]:.2f}) "
              f"mean_luma={l:.2f} bright_ratio={bright:.5f} warm_emissive_ratio={warm:.5f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())