"""
验证地形高度: KF demo.field 原始顶点 vs terrain_height.lua 查询
检查出生点附近的地形形状
"""
import struct
import math

FIELD_PATH = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\field\demo.field"

def main():
    with open(FIELD_PATH, "rb") as f:
        data = f.read()

    off = 0
    bnx = struct.unpack_from("<i", data, off)[0]; off += 4
    bnz = struct.unpack_from("<i", data, off)[0]; off += 4
    bsx, bsz = struct.unpack_from("<2f", data, off); off += 8
    vnum = struct.unpack_from("<i", data, off)[0]; off += 4

    cols = bnx + 1  # 101
    rows = bnz + 1  # 101
    print(f"Grid: {cols}x{rows}, block=({bsx},{bsz}), vertices={vnum}")

    # 读取所有顶点 (x, y, z)
    vertices = []
    for i in range(vnum):
        x, y, z = struct.unpack_from("<3f", data, off); off += 12
        vertices.append((x, y, z))

    # 验证顶点坐标范围
    xs = [v[0] for v in vertices]
    ys = [v[1] for v in vertices]
    zs = [v[2] for v in vertices]
    print(f"X range: [{min(xs):.2f}, {max(xs):.2f}]")
    print(f"Y range: [{min(ys):.2f}, {max(ys):.2f}]")
    print(f"Z range: [{min(zs):.2f}, {max(zs):.2f}]")

    # 验证顶点排列顺序
    print(f"\nFirst vertex (ix=0,iz=0): ({vertices[0][0]:.2f}, {vertices[0][1]:.2f}, {vertices[0][2]:.2f})")
    print(f"Vertex (ix=1,iz=0): ({vertices[1][0]:.2f}, {vertices[1][1]:.2f}, {vertices[1][2]:.2f})")
    print(f"Vertex (ix=0,iz=1): ({vertices[cols][0]:.2f}, {vertices[cols][1]:.2f}, {vertices[cols][2]:.2f})")
    print(f"Last vertex (ix=100,iz=100): ({vertices[-1][0]:.2f}, {vertices[-1][1]:.2f}, {vertices[-1][2]:.2f})")

    # 骑士出生点: KF (-82.58, 0, -95.42)
    kf_x, kf_z = -82.5767, -95.4176
    # 网格坐标
    gx = (kf_x - (-150)) / bsx  # = (kf_x + 150) / 3
    gz = (150 - kf_z) / bsz     # = (150 - kf_z) / 3
    print(f"\n=== 骑士出生点 ===")
    print(f"KF: ({kf_x:.4f}, 0, {kf_z:.4f})")
    print(f"Grid: gx={gx:.2f}, gz={gz:.2f}")
    print(f"Grid index: ix={int(gx)}, iz={int(gz)}")

    ix = int(gx)
    iz = int(gz)
    # 获取出生点附近 4 个角的高度
    idx00 = iz * cols + ix
    idx10 = iz * cols + ix + 1
    idx01 = (iz + 1) * cols + ix
    idx11 = (iz + 1) * cols + ix + 1
    v00 = vertices[idx00]
    v10 = vertices[idx10]
    v01 = vertices[idx01]
    v11 = vertices[idx11]
    print(f"Corner vertices:")
    print(f"  ({ix},{iz}): pos=({v00[0]:.2f}, {v00[1]:.4f}, {v00[2]:.2f})")
    print(f"  ({ix+1},{iz}): pos=({v10[0]:.2f}, {v10[1]:.4f}, {v10[2]:.2f})")
    print(f"  ({ix},{iz+1}): pos=({v01[0]:.2f}, {v01[1]:.4f}, {v01[2]:.2f})")
    print(f"  ({ix+1},{iz+1}): pos=({v11[0]:.2f}, {v11[1]:.4f}, {v11[2]:.2f})")

    # 双线性插值
    fx = gx - ix
    fz = gz - iz
    h = v00[1]*(1-fx)*(1-fz) + v10[1]*fx*(1-fz) + v01[1]*(1-fx)*fz + v11[1]*fx*fz
    print(f"Interpolated height: {h:.4f} (KF Y)")
    print(f"DSE Y = {h * 100:.1f}")

    # 打印出生点周围 ±10 格的高度轮廓 (沿 X 方向)
    print(f"\n=== X轴高度轮廓 (iz={iz}, ix={max(0,ix-15)}..{min(100,ix+15)}) ===")
    for x_idx in range(max(0, ix-15), min(cols, ix+16)):
        v = vertices[iz * cols + x_idx]
        marker = " <<<" if x_idx == ix else ""
        bar = "#" * int(v[1] * 4)
        print(f"  ix={x_idx:3d} x={v[0]:7.2f} y={v[1]:7.4f} {bar}{marker}")

    # 打印出生点周围 ±10 格的高度轮廓 (沿 Z 方向)
    print(f"\n=== Z轴高度轮廓 (ix={ix}, iz={max(0,iz-15)}..{min(100,iz+15)}) ===")
    for z_idx in range(max(0, iz-15), min(rows, iz+16)):
        v = vertices[z_idx * cols + ix]
        marker = " <<<" if z_idx == iz else ""
        bar = "#" * int(v[1] * 4)
        print(f"  iz={z_idx:3d} z={v[2]:7.2f} y={v[1]:7.4f} {bar}{marker}")

    # 打印整个地形的高度概览 (每10格采样)
    print(f"\n=== 地形高度概览 (每10格) ===")
    header = "     " + "".join(f"ix={x:3d} " for x in range(0, cols, 10))
    print(header)
    for z_idx in range(0, rows, 10):
        row_str = f"iz={z_idx:3d} "
        for x_idx in range(0, cols, 10):
            v = vertices[z_idx * cols + x_idx]
            row_str += f"{v[1]:6.2f} "
        print(row_str)

if __name__ == "__main__":
    main()
