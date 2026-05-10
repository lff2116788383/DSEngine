#!/usr/bin/env python3
"""Compare DX11 HDR, DX11 SDR, OpenGL vs KF original."""
from PIL import Image
import numpy as np

kf_path = "../../screenshots/kf_original_battle.png"  # relative to KF_Framework
shots = {
    "DX11_HDR": "../../../screenshots/dx11_hdr.png",
    "DX11_SDR": "../../../screenshots/dx11_sdr.png",
    "OpenGL":   "../../../screenshots/opengl.png",
}

import os, sys
os.chdir(os.path.dirname(os.path.abspath(__file__)))
# resolve to engine root
engine_root = os.path.abspath("../../..")
kf_path = os.path.join(engine_root, "examples/KF_Framework/screenshots/kf_original_battle.png")
shots = {
    "DX11_HDR": os.path.join(engine_root, "screenshots/dx11_hdr.png"),
    "DX11_SDR": os.path.join(engine_root, "screenshots/dx11_sdr.png"),
    "OpenGL":   os.path.join(engine_root, "screenshots/opengl.png"),
}

kf = np.array(Image.open(kf_path).resize((1280, 720)))[:, :, :3].astype(float)
imgs = {}
for k, p in shots.items():
    imgs[k] = np.array(Image.open(p))[:, :, :3].astype(float)

header = f"{'Pair':<25} {'RMSE':>6} {'Bright_A':>9} {'Bright_B':>9} {'Diff':>6} {'<5':>6} {'<20':>6} {'<80':>6}"
print(header)
print("-" * len(header))

def compare(name, a, b):
    diff = a - b
    rmse = np.sqrt(np.mean(diff ** 2))
    ma, mb = np.mean(a), np.mean(b)
    p5 = np.mean(np.abs(diff) < 5) * 100
    p20 = np.mean(np.abs(diff) < 20) * 100
    p80 = np.mean(np.abs(diff) < 80) * 100
    print(f"{name:<25} {rmse:6.1f} {ma:9.1f} {mb:9.1f} {ma - mb:+6.1f} {p5:5.1f}% {p20:5.1f}% {p80:5.1f}%")

# vs KF original
for name, img in imgs.items():
    compare(name + " vs KF", img, kf)

# cross-compare
pairs = [("DX11_HDR", "DX11_SDR"), ("DX11_HDR", "OpenGL"), ("DX11_SDR", "OpenGL")]
for a, b in pairs:
    compare(a + " vs " + b, imgs[a], imgs[b])

# Generate heatmaps
out_dir = os.path.join(engine_root, "screenshots")
for name, img in imgs.items():
    diff = np.abs(img - kf)
    mx = np.max(diff, axis=2)
    h = np.clip(mx * 3, 0, 255).astype(np.uint8)
    r = h
    g = np.clip(255 - h, 0, 255).astype(np.uint8)
    bl = np.zeros_like(h)
    hm = np.stack([r, g, bl], axis=2)
    out = os.path.join(out_dir, f"heatmap_{name.lower()}_vs_kf.png")
    Image.fromarray(hm).save(out)
    print(f"Heatmap: {out}")
