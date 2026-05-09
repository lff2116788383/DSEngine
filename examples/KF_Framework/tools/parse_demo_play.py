"""
解析 KF demo_play.log 并生成 Lua 输入回放表
KeyInfo struct (MSVC):
  float move_horizontal    (4)
  float move_vertical      (4)
  float rotation_horizontal(4)
  float rotation_vertical  (4)
  float zoom               (4)
  int   press_state        (4)
  int   trigger_state      (4)
  int   release_state      (4)
  short end_of_file        (2)
  -- padding: 2 bytes (MSVC struct alignment to 4-byte boundary)
Total: 36 bytes per frame
"""
import struct
import os

INPUT_PATH = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\input\demo_play.log"
OUT_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "script", "demo_play_data.lua")

def main():
    with open(INPUT_PATH, "rb") as f:
        data = f.read()

    print(f"File size: {len(data)} bytes")

    # Try both 34 and 36 byte frames
    for frame_size in [34, 36]:
        frames = []
        off = 0
        while off + frame_size <= len(data):
            if frame_size == 36:
                mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
                ps, ts, rs = struct.unpack_from("<3i", data, off + 20)
                eof = struct.unpack_from("<h", data, off + 32)[0]
            else:  # 34
                mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
                ps, ts, rs = struct.unpack_from("<3i", data, off + 20)
                eof = struct.unpack_from("<h", data, off + 32)[0]

            frames.append((mh, mv, rh, rv, zoom, ps, ts, rs, eof))
            off += frame_size
            if eof == 1:
                break

        print(f"\nFrame size {frame_size}: {len(frames)} frames parsed, last offset={off}")
        if frames:
            print(f"  First: mh={frames[0][0]:.2f} mv={frames[0][1]:.2f} rh={frames[0][2]:.2f} rv={frames[0][3]:.2f} eof={frames[0][8]}")
            # Find first non-zero frame
            for i, fr in enumerate(frames):
                if any(abs(v) > 0.001 for v in fr[:5]) or fr[5] != 0 or fr[6] != 0 or fr[7] != 0:
                    print(f"  First non-zero at frame {i}: mh={fr[0]:.2f} mv={fr[1]:.2f} rh={fr[2]:.2f} rv={fr[3]:.2f} ps={fr[5]} ts={fr[6]} rs={fr[7]} eof={fr[8]}")
                    break
            if frames[-1][8] == 1:
                print(f"  Last frame has end_of_file=1 (valid)")
            else:
                print(f"  Last frame eof={frames[-1][8]} (no terminator found)")

    # Use frame_size=36 (MSVC default alignment)
    frame_size = 36
    frames = []
    off = 0
    while off + frame_size <= len(data):
        mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
        ps, ts, rs = struct.unpack_from("<3i", data, off + 20)
        eof = struct.unpack_from("<h", data, off + 32)[0]
        frames.append((mh, mv, rh, rv, zoom, ps, ts, rs, eof))
        off += frame_size
        if eof == 1:
            break

    if not frames or frames[-1][8] != 1:
        # Try 34
        frame_size = 34
        frames = []
        off = 0
        while off + frame_size <= len(data):
            mh, mv, rh, rv, zoom = struct.unpack_from("<5f", data, off)
            ps, ts, rs = struct.unpack_from("<3i", data, off + 20)
            eof = struct.unpack_from("<h", data, off + 32)[0]
            frames.append((mh, mv, rh, rv, zoom, ps, ts, rs, eof))
            off += frame_size
            if eof == 1:
                break

    print(f"\nUsing frame_size={frame_size}, total frames={len(frames)}")

    # Generate Lua file
    with open(OUT_PATH, "w") as f:
        f.write("--------------------------------------------------------------------------------\n")
        f.write("-- DemoPlay 输入回放数据 (自动生成, 勿手动修改)\n")
        f.write("-- 来源: KF data/input/demo_play.log\n")
        f.write("-- 每帧: {move_h, move_v, rot_h, rot_v, zoom, press, trigger, release}\n")
        f.write("-- KF Input Key bits: kSubmit=1, kCancel=2, kLeft=4, kRight=8,\n")
        f.write("--   kLightAttack=16, kStrongAttack=32, kJump=64, kBlock=128, kSkill=256\n")
        f.write("--------------------------------------------------------------------------------\n\n")
        f.write("local DemoPlayData = {}\n\n")
        f.write(f"DemoPlayData.frame_count = {len(frames)}\n\n")
        f.write("-- {move_h, move_v, rot_h, rot_v, zoom, press, trigger, release}\n")
        f.write("DemoPlayData.frames = {\n")
        for fr in frames:
            if fr[8] == 1:
                break  # Don't include the terminator frame
            f.write(f"  {{{fr[0]:.4f},{fr[1]:.4f},{fr[2]:.4f},{fr[3]:.4f},{fr[4]:.4f},{fr[5]},{fr[6]},{fr[7]}}},\n")
        f.write("}\n\n")
        f.write("return DemoPlayData\n")

    print(f"Generated: {OUT_PATH}")

if __name__ == "__main__":
    main()
