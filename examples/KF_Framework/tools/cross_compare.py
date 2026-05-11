#!/usr/bin/env python3
"""Cross-compare all backends vs KF original."""
from PIL import Image
import numpy as np
import os

base = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'screenshots')

imgs = {}
for name, fn in [('KF', 'kf_original_battle.png'), ('OpenGL', 'dse_opengl.png'), ('DX11', 'dse_dx11.png'), ('Vulkan', 'dse_vulkan.png')]:
    path = os.path.join(base, fn)
    if not os.path.exists(path):
        print(f"[SKIP] {name}: {path} not found")
        continue
    imgs[name] = np.array(Image.open(path).resize((1280, 720)))[:, :, :3].astype(float)

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
    print(f"{name:<25} {rmse:6.1f} {ma:9.1f} {mb:9.1f} {ma-mb:+6.1f} {p5:5.1f}% {p20:5.1f}% {p80:5.1f}%")

# vs KF
if 'KF' in imgs:
    for name in ['OpenGL', 'DX11', 'Vulkan']:
        if name in imgs:
            compare(f"{name} vs KF", imgs[name], imgs['KF'])

print()
# cross-compare
for a, b in [('OpenGL', 'DX11'), ('OpenGL', 'Vulkan'), ('DX11', 'Vulkan')]:
    if a in imgs and b in imgs:
        compare(f"{a} vs {b}", imgs[a], imgs[b])
