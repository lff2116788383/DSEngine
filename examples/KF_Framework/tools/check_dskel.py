#!/usr/bin/env python3
"""检查 .dskel 文件的骨骼数和父子关系，用于诊断骨骼不匹配问题。"""
import struct, sys, pathlib

def read_dskel(path):
    data = pathlib.Path(path).read_bytes()
    magic = data[0:4]
    if magic != b'DSES':
        print(f"  ERROR: bad magic {magic!r}")
        return
    version, bone_count = struct.unpack_from('<II', data, 4)
    print(f"  magic=DSES  version={version}  bone_count={bone_count}")
    offset = 12
    for i in range(bone_count):
        parent = struct.unpack_from('<i', data, offset)[0]
        # inverse_bind_matrix: 64 bytes, local_transform: 64 bytes
        print(f"    bone[{i:2d}] parent={parent:3d}")
        offset += 4 + 64 + 64

if __name__ == '__main__':
    for p in sys.argv[1:]:
        print(f"\n=== {p} ===")
        read_dskel(p)
