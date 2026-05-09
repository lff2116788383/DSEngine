"""
从 KF demo.field 提取地形高度网格，生成 Lua 高度表
KF FieldCollider 格式:
  int block_number_x (100)
  int block_number_z (100)
  Vector2 block_size (3.0, 3.0)
  int vertex_number (101*101 = 10201)
  Vector3[] vertexes (10201 个)
顶点排列: Z 外循环 (0..100), X 内循环 (0..100)
KF 坐标: x ∈ [-150, 150], z ∈ [-150, 150]
DSE 坐标: DSE_x = KF_x * 100, DSE_z = -KF_z * 100, DSE_y = KF_y * 100
"""
import struct
import os

FIELD_PATH = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\field\demo.field"
OUT_PATH = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                        "script", "terrain_height.lua")

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
    assert vnum == cols * rows, f"vertex count mismatch: {vnum} != {cols*rows}"

    print(f"Grid: {cols}x{rows}, block_size=({bsx},{bsz}), vertices={vnum}")

    # 读取所有顶点
    heights = []  # KF Y values, row-major (z outer, x inner)
    for i in range(vnum):
        x, y, z = struct.unpack_from("<3f", data, off); off += 12
        heights.append(y)

    # 验证范围
    print(f"Height range: [{min(heights):.4f}, {max(heights):.4f}]")

    # 生成 Lua 文件
    # 存储为 1D 数组 (Lua 1-indexed), 按 row-major (z=0..100, x=0..100)
    # Lua 访问: heights[iz * cols + ix + 1]
    with open(OUT_PATH, "w") as f:
        f.write("--------------------------------------------------------------------------------\n")
        f.write("-- 地形高度表 (自动生成, 勿手动修改)\n")
        f.write("-- 来源: KF demo.field (FieldCollider 顶点数据)\n")
        f.write("-- 网格: 101x101, block_size=3.0, 范围 KF[-150,150]\n")
        f.write("-- KF 坐标: grid_x = (kf_x + 150) / 3, grid_z = (kf_z + 150) / 3\n")
        f.write("-- DSE 坐标: kf_x = dse_x / 100, kf_z = -dse_z / 100\n")
        f.write("-- 高度: KF_y → DSE_y = KF_y * 100\n")
        f.write("--------------------------------------------------------------------------------\n\n")
        f.write("local TerrainHeight = {}\n\n")
        f.write(f"TerrainHeight.cols = {cols}\n")
        f.write(f"TerrainHeight.rows = {rows}\n")
        f.write(f"TerrainHeight.block_size = {bsx}\n")
        f.write(f"TerrainHeight.origin_x = -150.0  -- KF min X\n")
        f.write(f"TerrainHeight.origin_z = -150.0  -- KF min Z\n")
        f.write(f"TerrainHeight.scale = 100.0       -- KF → DSE 缩放\n\n")

        f.write("-- 高度数据 (KF Y 值, row-major: z=0..100 外循环, x=0..100 内循环)\n")
        f.write("TerrainHeight.data = {\n")
        for iz in range(rows):
            row_start = iz * cols
            row_vals = heights[row_start:row_start + cols]
            # 每行写一行, 保持可读性
            vals_str = ",".join(f"{v:.4f}" for v in row_vals)
            f.write(f"  {vals_str},\n")
        f.write("}\n\n")

        # 高度查询函数 (DSE 世界坐标 → DSE Y)
        f.write("--------------------------------------------------------------------------------\n")
        f.write("-- 根据 DSE 世界坐标 (wx, wz) 查询地形高度 (返回 DSE Y)\n")
        f.write("-- 使用双线性插值\n")
        f.write("--------------------------------------------------------------------------------\n")
        f.write("function TerrainHeight.get_height(wx, wz)\n")
        f.write("    -- DSE → KF 坐标\n")
        f.write("    local kf_x = wx / TerrainHeight.scale\n")
        f.write("    local kf_z = -wz / TerrainHeight.scale\n")
        f.write("\n")
        f.write("    -- KF → 网格索引 (浮点)\n")
        f.write("    local gx = (kf_x - TerrainHeight.origin_x) / TerrainHeight.block_size\n")
        f.write("    local gz = (kf_z - TerrainHeight.origin_z) / TerrainHeight.block_size\n")
        f.write("\n")
        f.write("    -- 钳制到网格范围\n")
        f.write("    local max_idx = TerrainHeight.cols - 1  -- 100\n")
        f.write("    if gx < 0 then gx = 0 end\n")
        f.write("    if gx > max_idx then gx = max_idx end\n")
        f.write("    if gz < 0 then gz = 0 end\n")
        f.write("    if gz > max_idx then gz = max_idx end\n")
        f.write("\n")
        f.write("    -- 双线性插值\n")
        f.write("    local ix = math.floor(gx)\n")
        f.write("    local iz = math.floor(gz)\n")
        f.write("    local fx = gx - ix\n")
        f.write("    local fz = gz - iz\n")
        f.write("\n")
        f.write("    -- 防止越界\n")
        f.write("    if ix >= max_idx then ix = max_idx - 1; fx = 1.0 end\n")
        f.write("    if iz >= max_idx then iz = max_idx - 1; fz = 1.0 end\n")
        f.write("\n")
        f.write("    -- 四个角的高度 (Lua 1-indexed)\n")
        f.write("    local d = TerrainHeight.data\n")
        f.write("    local c = TerrainHeight.cols\n")
        f.write("    local h00 = d[iz * c + ix + 1]\n")
        f.write("    local h10 = d[iz * c + ix + 2]\n")
        f.write("    local h01 = d[(iz + 1) * c + ix + 1]\n")
        f.write("    local h11 = d[(iz + 1) * c + ix + 2]\n")
        f.write("\n")
        f.write("    -- 双线性插值\n")
        f.write("    local h = h00 * (1 - fx) * (1 - fz)\n")
        f.write("            + h10 * fx * (1 - fz)\n")
        f.write("            + h01 * (1 - fx) * fz\n")
        f.write("            + h11 * fx * fz\n")
        f.write("\n")
        f.write("    -- KF Y → DSE Y\n")
        f.write("    return h * TerrainHeight.scale\n")
        f.write("end\n\n")

        f.write("return TerrainHeight\n")

    print(f"Generated: {OUT_PATH}")
    print(f"  {vnum} height values in {rows} rows × {cols} cols")

if __name__ == "__main__":
    main()
