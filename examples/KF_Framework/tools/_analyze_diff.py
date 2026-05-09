from PIL import Image
import numpy as np

kf = np.array(Image.open("screenshots/kf_original_battle.png").convert("RGB")).astype(float)
dse = np.array(Image.open("screenshots/dse_battle.png").convert("RGB")).astype(float)
h = min(kf.shape[0], dse.shape[0])
w = min(kf.shape[1], dse.shape[1])
kf = kf[:h,:w,:]
dse = dse[:h,:w,:]
diff = dse - kf

for i, ch in enumerate(["R","G","B"]):
    print(f"{ch}: mean_signed={diff[:,:,i].mean():.1f}, abs={np.abs(diff[:,:,i]).mean():.1f}")

# Sky (top 100 rows)
print(f"Sky  KF:  R={kf[:100,:,0].mean():.0f} G={kf[:100,:,1].mean():.0f} B={kf[:100,:,2].mean():.0f}")
print(f"Sky  DSE: R={dse[:100,:,0].mean():.0f} G={dse[:100,:,1].mean():.0f} B={dse[:100,:,2].mean():.0f}")

# Ground (bottom 200 rows)
print(f"Gnd  KF:  R={kf[-200:,:,0].mean():.0f} G={kf[-200:,:,1].mean():.0f} B={kf[-200:,:,2].mean():.0f}")
print(f"Gnd  DSE: R={dse[-200:,:,0].mean():.0f} G={dse[-200:,:,1].mean():.0f} B={dse[-200:,:,2].mean():.0f}")

# Middle band (buildings area)
print(f"Mid  KF:  R={kf[200:500,:,0].mean():.0f} G={kf[200:500,:,1].mean():.0f} B={kf[200:500,:,2].mean():.0f}")
print(f"Mid  DSE: R={dse[200:500,:,0].mean():.0f} G={dse[200:500,:,1].mean():.0f} B={dse[200:500,:,2].mean():.0f}")

# HUD area (top-left corner 200x100)
print(f"HUD  KF:  R={kf[:100,:200,0].mean():.0f} G={kf[:100,:200,1].mean():.0f} B={kf[:100,:200,2].mean():.0f}")
print(f"HUD  DSE: R={dse[:100,:200,0].mean():.0f} G={dse[:100,:200,1].mean():.0f} B={dse[:100,:200,2].mean():.0f}")

# Check actual texture colors
tex = np.array(Image.open("assets/textures/demoField.jpg").convert("RGB")).astype(float)
print(f"\ndemoField.jpg texture avg: R={tex[:,:,0].mean():.0f} G={tex[:,:,1].mean():.0f} B={tex[:,:,2].mean():.0f}")
print(f"demoField.jpg center px: R={tex[tex.shape[0]//2,tex.shape[1]//2,0]:.0f} G={tex[tex.shape[0]//2,tex.shape[1]//2,1]:.0f} B={tex[tex.shape[0]//2,tex.shape[1]//2,2]:.0f}")

sky = np.array(Image.open("assets/textures/skybox000.jpg").convert("RGB")).astype(float)
print(f"skybox000.jpg avg: R={sky[:,:,0].mean():.0f} G={sky[:,:,1].mean():.0f} B={sky[:,:,2].mean():.0f}")

# Per-channel ratios DSE/KF
print("\nDSE/KF ratios:")
for label, kf_region, dse_region in [
    ("Sky", kf[:100,:,:], dse[:100,:,:]),
    ("Gnd", kf[-200:,:,:], dse[-200:,:,:]),
    ("Mid", kf[200:500,:,:], dse[200:500,:,:]),
]:
    for i, ch in enumerate(["R","G","B"]):
        kv = kf_region[:,:,i].mean()
        dv = dse_region[:,:,i].mean()
        print(f"  {label} {ch}: {dv/kv:.3f}" if kv > 1 else f"  {label} {ch}: N/A")

# Check a pure sky pixel (no HUD, top-center)
r0, c0 = 50, 640
print(f"\nPure sky pixel (r={r0},c={c0}):")
print(f"  KF:  R={kf[r0,c0,0]:.0f} G={kf[r0,c0,1]:.0f} B={kf[r0,c0,2]:.0f}")
print(f"  DSE: R={dse[r0,c0,0]:.0f} G={dse[r0,c0,1]:.0f} B={dse[r0,c0,2]:.0f}")
