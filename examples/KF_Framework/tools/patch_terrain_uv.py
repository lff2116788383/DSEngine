"""
修补 demoField.dmesh 的 UV V 坐标: V → 1.0 - V
原因: kf_to_gltf.py 翻转了顶点 Z 但未翻转 UV V，导致纹理沿 Z 方向镜像
"""
import struct
import shutil

DMESH_PATH = r"c:\Users\wenbilin\Desktop\Engine\DSEngine\examples\KF_Framework\cooked\demoField.dmesh"
BACKUP_PATH = DMESH_PATH + ".bak"

def main():
    # 备份
    shutil.copy2(DMESH_PATH, BACKUP_PATH)
    print(f"Backup: {BACKUP_PATH}")

    with open(DMESH_PATH, "rb") as f:
        data = bytearray(f.read())

    # 解析 header
    ver = struct.unpack_from("<I", data, 4)[0]
    vc = struct.unpack_from("<I", data, 8)[0]
    vdo = struct.unpack_from("<Q", data, 24)[0]
    stride_floats = 24 if ver >= 2 else 20
    stride_bytes = stride_floats * 4

    print(f"Vertices: {vc}, stride: {stride_floats} floats, vertex_data_offset: {vdo}")

    # dmesh v1 layout: pos(3f) normal(3f) uv(2f) weights(4f) joints(4i) tangent(4f)
    # UV offset within vertex = 6 floats = 24 bytes
    uv_offset = 6 * 4  # 24 bytes after vertex start

    flipped = 0
    for i in range(vc):
        v_off = vdo + i * stride_bytes + uv_offset
        u, v = struct.unpack_from("<2f", data, v_off)
        new_v = 1.0 - v
        struct.pack_into("<f", data, v_off + 4, new_v)
        flipped += 1

    with open(DMESH_PATH, "wb") as f:
        f.write(data)

    print(f"Flipped V for {flipped} vertices")
    print(f"Saved: {DMESH_PATH}")

    # 验证
    with open(DMESH_PATH, "rb") as f:
        data2 = f.read()
    for i in [0, 100, 5050, 8203, 10200]:
        if i >= vc: continue
        off = vdo + i * stride_bytes
        x, y, z = struct.unpack_from("<3f", data2, off)
        u, v = struct.unpack_from("<2f", data2, off + uv_offset)
        print(f"  [{i:5d}] pos=({x:8.2f},{y:7.2f},{z:8.2f})  uv=({u:.4f},{v:.4f})")

if __name__ == "__main__":
    main()
