"""
检查 KF 出生坐标 vs DSE 出生坐标，排查位置差异
"""
import struct
import os

# 1. 读取 KF demo.player 文件
player_path = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\stage\demo.player"
with open(player_path, "rb") as f:
    data = f.read()

print(f"=== demo.player ({len(data)} bytes) ===")
print(f"Hex: {data.hex()}")

# 尝试解析为 float 序列
print("\nAs floats (4-byte each):")
for i in range(0, len(data) - 3, 4):
    val = struct.unpack_from("<f", data, i)[0]
    print(f"  offset {i:3d}: {val:.6f}")

# 尝试解析为 3 个 float (position)
if len(data) >= 12:
    x, y, z = struct.unpack_from("<3f", data, 0)
    print(f"\nFirst 3 floats as position: ({x:.4f}, {y:.4f}, {z:.4f})")
    print(f"KF→DSE transform: ({x*100:.1f}, {y*100:.1f}, {-z*100:.1f})")

# 2. 读取 demo.enemy 文件
enemy_path = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\stage\demo.enemy"
if os.path.exists(enemy_path):
    with open(enemy_path, "rb") as f:
        edata = f.read()
    print(f"\n=== demo.enemy ({len(edata)} bytes) ===")
    print(f"Hex: {edata.hex()}")
    print("\nAs floats:")
    for i in range(0, min(len(edata), 80) - 3, 4):
        val = struct.unpack_from("<f", edata, i)[0]
        print(f"  offset {i:3d}: {val:.6f}")

# 3. 读取 demo.stage 文件
stage_path = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\stage\demo.stage"
if os.path.exists(stage_path):
    with open(stage_path, "rb") as f:
        sdata = f.read()
    print(f"\n=== demo.stage ({len(sdata)} bytes) ===")
    print(f"Hex (first 200): {sdata[:200].hex()}")

# 4. DSE 当前出生坐标
print("\n=== DSE player.lua spawn ===")
print("  spawn: (-8258, terrain_y, 9542)")
print("  KF origin: (-82.5767, 0, -95.4176) → DSE: x*100=-8258, z=-(-95.4176)*100=9542")

# 5. 检查 demo.field 的顶点范围
field_path = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\field\demo.field"
if os.path.exists(field_path):
    with open(field_path, "rb") as f:
        fdata = f.read()
    print(f"\n=== demo.field ({len(fdata)} bytes) ===")
    # 尝试找出顶点范围 - 从 mesh 数据解析
    # field 文件格式未知，先看前 100 字节
    print(f"Header hex (first 64): {fdata[:64].hex()}")
    # 尝试读取前几个 int 作为 header
    if len(fdata) >= 16:
        vals = struct.unpack_from("<4i", fdata, 0)
        print(f"First 4 ints: {vals}")
