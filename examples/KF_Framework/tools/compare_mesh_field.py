"""
对比 demoField.dmesh 顶点位置 vs demo.field 顶点位置
"""
import struct

DMESH_PATH = r"c:\Users\wenbilin\Desktop\Engine\DSEngine\examples\KF_Framework\cooked\demoField.dmesh"
FIELD_PATH = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\field\demo.field"

def main():
    # --- 读取 dmesh ---
    with open(DMESH_PATH, "rb") as f:
        dmesh = f.read()

    # MeshHeader (#pragma pack(1))
    magic = dmesh[0:4]
    ver = struct.unpack_from("<I", dmesh, 4)[0]
    vc = struct.unpack_from("<I", dmesh, 8)[0]
    ic = struct.unpack_from("<I", dmesh, 12)[0]
    sc = struct.unpack_from("<I", dmesh, 16)[0]
    attr = struct.unpack_from("<I", dmesh, 20)[0]
    vdo = struct.unpack_from("<Q", dmesh, 24)[0]
    ido = struct.unpack_from("<Q", dmesh, 32)[0]
    sdo = struct.unpack_from("<Q", dmesh, 40)[0]
    
    stride_floats = 24 if ver >= 2 else 20  # floats per vertex
    stride_bytes = stride_floats * 4
    
    print(f"=== demoField.dmesh ===")
    print(f"magic={magic} ver={ver} verts={vc} indices={ic} submeshes={sc} attr=0x{attr:X}")
    print(f"vertex_data_offset={vdo} index_data_offset={ido} submesh_data_offset={sdo}")
    print(f"stride={stride_floats} floats ({stride_bytes} bytes)")
    
    # 读取 dmesh 顶点位置
    dmesh_positions = []
    for i in range(vc):
        off = vdo + i * stride_bytes
        x, y, z = struct.unpack_from("<3f", dmesh, off)
        dmesh_positions.append((x, y, z))
    
    # --- 读取 demo.field ---
    with open(FIELD_PATH, "rb") as f:
        field = f.read()
    
    off = 0
    bnx = struct.unpack_from("<i", field, off)[0]; off += 4
    bnz = struct.unpack_from("<i", field, off)[0]; off += 4
    bsx, bsz = struct.unpack_from("<2f", field, off); off += 8
    vnum = struct.unpack_from("<i", field, off)[0]; off += 4
    
    field_positions = []
    for i in range(vnum):
        x, y, z = struct.unpack_from("<3f", field, off); off += 12
        field_positions.append((x, y, z))
    
    print(f"\n=== demo.field ===")
    print(f"grid={bnx}x{bnz} block=({bsx},{bsz}) verts={vnum}")
    
    # --- 比较 ---
    print(f"\n=== 比较 ===")
    print(f"dmesh verts: {len(dmesh_positions)}")
    print(f"field verts: {len(field_positions)}")
    
    # 打印前 5 个顶点
    print("\nFirst 5 vertices:")
    for i in range(min(5, len(dmesh_positions))):
        dp = dmesh_positions[i]
        fp = field_positions[i] if i < len(field_positions) else (0,0,0)
        diff = (dp[0]-fp[0], dp[1]-fp[1], dp[2]-fp[2])
        print(f"  [{i}] dmesh=({dp[0]:8.2f},{dp[1]:8.4f},{dp[2]:8.2f})  field=({fp[0]:8.2f},{fp[1]:8.4f},{fp[2]:8.2f})  diff=({diff[0]:.4f},{diff[1]:.4f},{diff[2]:.4f})")
    
    # 检查出生点附近 (grid 22,81 → vertex index = 81*101+22 = 8203)
    spawn_idx = 81 * 101 + 22
    print(f"\nSpawn point vertex (index {spawn_idx}):")
    if spawn_idx < len(dmesh_positions) and spawn_idx < len(field_positions):
        dp = dmesh_positions[spawn_idx]
        fp = field_positions[spawn_idx]
        print(f"  dmesh: ({dp[0]:.2f}, {dp[1]:.4f}, {dp[2]:.2f})")
        print(f"  field: ({fp[0]:.2f}, {fp[1]:.4f}, {fp[2]:.2f})")
    
    # Y 差异统计
    if len(dmesh_positions) == len(field_positions):
        max_dy = 0
        total_dy = 0
        mismatches = 0
        for i in range(len(dmesh_positions)):
            dy = abs(dmesh_positions[i][1] - field_positions[i][1])
            total_dy += dy
            if dy > max_dy:
                max_dy = dy
            if dy > 0.01:
                mismatches += 1
        avg_dy = total_dy / len(dmesh_positions)
        print(f"\nY height difference:")
        print(f"  Max |dy|: {max_dy:.4f}")
        print(f"  Avg |dy|: {avg_dy:.4f}")
        print(f"  Mismatches (|dy|>0.01): {mismatches}/{len(dmesh_positions)}")
        
        # XZ 差异
        max_dxz = 0
        for i in range(len(dmesh_positions)):
            dx = abs(dmesh_positions[i][0] - field_positions[i][0])
            dz = abs(dmesh_positions[i][2] - field_positions[i][2])
            dxz = max(dx, dz)
            if dxz > max_dxz:
                max_dxz = dxz
        print(f"  Max |dx| or |dz|: {max_dxz:.4f}")
    else:
        print("\n  WARNING: vertex count mismatch, cannot compare directly!")
        # 尝试用最近点匹配 (检查 dmesh 出生点附近的实际高度)
        spawn_kf = (-82.58, -95.42)
        print(f"\n  Looking for dmesh vertex near KF spawn ({spawn_kf[0]:.2f}, ?, {spawn_kf[1]:.2f}):")
        best_dist = 999
        best_v = None
        for i, dp in enumerate(dmesh_positions):
            dist = ((dp[0] - spawn_kf[0])**2 + (dp[2] - spawn_kf[1])**2)**0.5
            if dist < best_dist:
                best_dist = dist
                best_v = (i, dp)
        if best_v:
            print(f"    Nearest: [{best_v[0]}] ({best_v[1][0]:.2f}, {best_v[1][1]:.4f}, {best_v[1][2]:.2f}) dist={best_dist:.2f}")
    
    # dmesh Y 范围
    dmesh_ys = [p[1] for p in dmesh_positions]
    print(f"\ndmesh Y range: [{min(dmesh_ys):.4f}, {max(dmesh_ys):.4f}]")
    
    # 出生点附近 X 轮廓 (从 dmesh)
    print(f"\ndmesh X轴轮廓 near spawn (iz≈81):")
    for ix in range(max(0, 22-10), min(101, 22+11)):
        idx = 81 * 101 + ix
        if idx < len(dmesh_positions):
            dp = dmesh_positions[idx]
            marker = " <<<" if ix == 22 else ""
            print(f"  ix={ix:3d} ({dp[0]:7.2f}, {dp[1]:7.4f}, {dp[2]:7.2f}){marker}")

if __name__ == "__main__":
    main()
