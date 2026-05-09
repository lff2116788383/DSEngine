#!/usr/bin/env python3
"""Parse KF demo_play.log binary and regenerate demo_play_data.lua"""
import struct, sys
from pathlib import Path

LOG_PATH = Path(r"C:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\input\demo_play.log")
OUT_PATH = Path(__file__).resolve().parent.parent / "script" / "demo_play_data.lua"

data = LOG_PATH.read_bytes()
print(f"File size: {len(data)} bytes")

# Try different struct sizes
for sz in [36, 34, 32]:
    n = len(data) // sz
    rem = len(data) % sz
    if rem == 0:
        print(f"sizeof={sz}: {n} frames exactly")
    else:
        print(f"sizeof={sz}: {n} frames + {rem} bytes remainder")

# KeyInfo: 5 floats(20) + 3 ints(12) + 1 short(2) + 2 padding = 36 bytes
RECORD_SIZE = 36
n_frames = len(data) // RECORD_SIZE

frames = []
eof_frame = n_frames
for i in range(n_frames):
    off = i * RECORD_SIZE
    mh, mv, rh, rv, zm = struct.unpack_from('<5f', data, off)
    pr, tr, rl = struct.unpack_from('<3i', data, off + 20)
    eof = struct.unpack_from('<h', data, off + 32)[0]
    frames.append((mh, mv, rh, rv, zm, pr, tr, rl, eof))
    if eof != 0:
        eof_frame = i
        break

print(f"\nTotal frames (before EOF): {eof_frame}")
nonzero_move = sum(1 for f in frames[:eof_frame] if abs(f[0]) > 0.001 or abs(f[1]) > 0.001)
print(f"Frames with movement: {nonzero_move}/{eof_frame}")

# Dump first/last frames with movement
for i, f in enumerate(frames[:eof_frame]):
    if abs(f[0]) > 0.001 or abs(f[1]) > 0.001:
        print(f"First move frame {i}: mh={f[0]:+.4f} mv={f[1]:+.4f}")
        break

# If sizeof=36 gives 0 movement, try 34
if nonzero_move == 0:
    print("\n--- Trying sizeof=34 ---")
    RECORD_SIZE = 34
    n_frames = len(data) // RECORD_SIZE
    frames = []
    eof_frame = n_frames
    for i in range(n_frames):
        off = i * RECORD_SIZE
        mh, mv, rh, rv, zm = struct.unpack_from('<5f', data, off)
        pr, tr, rl = struct.unpack_from('<3i', data, off + 20)
        eof = struct.unpack_from('<h', data, off + 32)[0]
        frames.append((mh, mv, rh, rv, zm, pr, tr, rl, eof))
        if eof != 0:
            eof_frame = i
            break
    print(f"Total frames (before EOF): {eof_frame}")
    nonzero_move = sum(1 for f in frames[:eof_frame] if abs(f[0]) > 0.001 or abs(f[1]) > 0.001)
    print(f"Frames with movement: {nonzero_move}/{eof_frame}")
    for i, f in enumerate(frames[:eof_frame]):
        if abs(f[0]) > 0.001 or abs(f[1]) > 0.001:
            print(f"First move frame {i}: mh={f[0]:+.4f} mv={f[1]:+.4f}")
            break

# Show first 20 frames
print(f"\n--- First 20 frames ---")
for i in range(min(20, eof_frame)):
    f = frames[i]
    tag = "MOVE" if abs(f[0]) > 0.001 or abs(f[1]) > 0.001 else ""
    print(f"  [{i:3d}] mh={f[0]:+.4f} mv={f[1]:+.4f} rh={f[2]:+.4f} rv={f[3]:+.4f} zm={f[4]:+.4f} pr={f[5]} tr={f[6]} rl={f[7]} eof={f[8]} {tag}")

# Generate Lua output
if "--gen" in sys.argv:
    lines = []
    lines.append("--------------------------------------------------------------------------------")
    lines.append("-- DemoPlay 输入回放数据 (自动生成, 勿手动修改)")
    lines.append("-- 来源: KF data/input/demo_play.log")
    lines.append("-- 每帧: {move_h, move_v, rot_h, rot_v, zoom, press, trigger, release}")
    lines.append("-- KF Input Key bits: kSubmit=1, kCancel=2, kLeft=4, kRight=8,")
    lines.append("--   kLightAttack=16, kStrongAttack=32, kJump=64, kBlock=128, kSkill=256")
    lines.append("--------------------------------------------------------------------------------")
    lines.append("")
    lines.append("local DemoPlayData = {}")
    lines.append("")
    lines.append(f"DemoPlayData.frame_count = {eof_frame}")
    lines.append("")
    lines.append("-- {move_h, move_v, rot_h, rot_v, zoom, press, trigger, release}")
    lines.append("DemoPlayData.frames = {")
    for i in range(eof_frame):
        f = frames[i]
        lines.append(f"  {{{f[0]:.4f},{f[1]:.4f},{f[2]:.4f},{f[3]:.4f},{f[4]:.4f},{f[5]},{f[6]},{f[7]}}},")
    lines.append("}")
    lines.append("")
    lines.append("return DemoPlayData")
    lines.append("")
    OUT_PATH.write_text("\n".join(lines), encoding="utf-8")
    print(f"\nGenerated: {OUT_PATH} ({eof_frame} frames)")
