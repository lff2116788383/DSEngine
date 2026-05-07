#!/usr/bin/env python3
"""检查 .danim 文件头信息，用于诊断动画加载问题。"""
import struct, sys, pathlib

def read_danim(path):
    data = pathlib.Path(path).read_bytes()
    magic = data[0:4]
    if magic != b'DSEA':
        print(f"  ERROR: bad magic {magic!r}")
        return
    version, = struct.unpack_from('<I', data, 4)
    duration, = struct.unpack_from('<f', data, 8)
    channel_count, = struct.unpack_from('<I', data, 12)
    print(f"  magic=DSEA  version={version}  duration={duration:.4f}s  channels={channel_count}")
    # AnimChannelDesc: int(4) + 3*uint32(12) + 4*uint64(32) = 48 bytes
    ch_offset = 16
    ch_size = 48
    for i in range(min(channel_count, 10)):  # print first 10
        target = struct.unpack_from('<i', data, ch_offset)[0]
        pos_keys = struct.unpack_from('<I', data, ch_offset + 4)[0]
        rot_keys = struct.unpack_from('<I', data, ch_offset + 8)[0]
        scl_keys = struct.unpack_from('<I', data, ch_offset + 12)[0]
        time_off = struct.unpack_from('<Q', data, ch_offset + 16)[0]
        pos_off = struct.unpack_from('<Q', data, ch_offset + 24)[0]
        rot_off = struct.unpack_from('<Q', data, ch_offset + 32)[0]
        scl_off = struct.unpack_from('<Q', data, ch_offset + 40)[0]
        print(f"    ch[{i:2d}] target_bone={target:3d}  pos={pos_keys}  rot={rot_keys}  scl={scl_keys}")
        ch_offset += ch_size
    if channel_count > 10:
        print(f"    ... ({channel_count - 10} more channels)")

if __name__ == '__main__':
    for p in sys.argv[1:]:
        print(f"\n=== {p} ===")
        read_danim(p)
