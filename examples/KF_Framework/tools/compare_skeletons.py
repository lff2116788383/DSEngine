#!/usr/bin/env python3
"""比较两个 .dskel 文件的骨骼拓扑和 inverse_bind_matrix，诊断骨骼不匹配原因。

骨骼不匹配根因说明:
  DSEngine 的 AssetBuilder 使用 Assimp 分别独立导入 mesh FBX 和 animation FBX。
  Assimp 按各自 FBX 中的节点遍历顺序分配骨骼索引，导致:
    - Paladin mesh FBX → 54 骨骼，root 在 index 51
    - Sword And Shield animation FBX → 52 骨骼，root 在 index 0
  解决方案: 编译动画时应以 mesh 骨架为参考，或在运行时按骨骼名称重映射。
"""
import struct, sys, pathlib, numpy as np

def load_skel(path):
    data = pathlib.Path(path).read_bytes()
    assert data[0:4] == b'DSES', f"bad magic in {path}"
    version, bone_count = struct.unpack_from('<II', data, 4)
    parents = []
    inv_binds = []
    local_transforms = []
    offset = 12
    for i in range(bone_count):
        parent = struct.unpack_from('<i', data, offset)[0]
        parents.append(parent)
        inv_bind = np.array(struct.unpack_from('<16f', data, offset + 4)).reshape(4, 4)
        inv_binds.append(inv_bind)
        local_t = np.array(struct.unpack_from('<16f', data, offset + 4 + 64)).reshape(4, 4)
        local_transforms.append(local_t)
        offset += 4 + 64 + 64
    return {'bone_count': bone_count, 'parents': parents, 'inv_binds': inv_binds, 'locals': local_transforms}

def find_roots(parents):
    return [i for i, p in enumerate(parents) if p < 0]

def build_children(parents):
    children = [[] for _ in range(len(parents))]
    for i, p in enumerate(parents):
        if p >= 0:
            children[p].append(i)
    return children

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: compare_skeletons.py <skel_a.dskel> <skel_b.dskel>")
        sys.exit(1)
    a = load_skel(sys.argv[1])
    b = load_skel(sys.argv[2])
    print(f"Skeleton A: {sys.argv[1]}  bones={a['bone_count']}")
    print(f"Skeleton B: {sys.argv[2]}  bones={b['bone_count']}")
    print(f"\nRoots A: {find_roots(a['parents'])}")
    print(f"Roots B: {find_roots(b['parents'])}")
    
    ca = build_children(a['parents'])
    cb = build_children(b['parents'])
    print(f"\nChild counts A: {[len(c) for c in ca]}")
    print(f"Child counts B: {[len(c) for c in cb]}")
    
    # Translation comparison (column 3 of inv_bind = bone world position)
    print(f"\nTranslation comparison (inv_bind_matrix[3,0:3]):")
    for i in range(min(a['bone_count'], b['bone_count'])):
        ta = a['inv_binds'][i][:3, 3]
        tb = b['inv_binds'][i][:3, 3]
        dist = np.linalg.norm(ta - tb)
        match = "✓" if dist < 0.01 else f"✗ dist={dist:.2f}"
        print(f"  [{i:2d}] A=({ta[0]:7.2f},{ta[1]:7.2f},{ta[2]:7.2f})  B=({tb[0]:7.2f},{tb[1]:7.2f},{tb[2]:7.2f})  {match}")
