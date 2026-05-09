"""
验证 demo.stage 原始坐标 vs kf_demo_stage.json 坐标
确认装饰物 Z 方向是否一致
"""
import struct, json

STAGE_PATH = r"c:\Users\wenbilin\Desktop\temp_analysis\KF_Framework\data\stage\demo.stage"
JSON_PATH = r"c:\Users\wenbilin\Desktop\Engine\DSEngine\examples\KF_Framework\scenes\kf_demo_stage.json"

with open(STAGE_PATH, "rb") as f:
    d = f.read()

off = 0
total = struct.unpack_from('<i', d, off)[0]; off += 4
print(f"demo.stage: total_models={total}")

kf_models = []
for i in range(total):
    name_len = struct.unpack_from('<i', d, off)[0]; off += 4
    name = d[off:off+name_len].decode('ascii', errors='replace'); off += name_len
    has_tf = struct.unpack_from('<i', d, off)[0]; off += 4
    if has_tf:
        x, y, z = struct.unpack_from('<3f', d, off); off += 12
        rx, ry, rz, rw = struct.unpack_from('<4f', d, off); off += 16
        sx, sy, sz = struct.unpack_from('<3f', d, off); off += 12
        # KF→DSE 坐标: x*100, y*100, -z*100
        dse_x = x * 100
        dse_y = y * 100
        dse_z = -z * 100
        kf_models.append((name, (x,y,z), (dse_x, dse_y, dse_z)))
        print(f"  [{i}] {name}: KF({x:.4f},{y:.4f},{z:.4f}) → DSE({dse_x:.1f},{dse_y:.1f},{dse_z:.1f})")
    else:
        kf_models.append((name, None, None))
        print(f"  [{i}] {name}: no transform")

# 比较 JSON
with open(JSON_PATH, 'r') as f:
    scene = json.load(f)

print(f"\nkf_demo_stage.json: {len(scene['entities'])} entities")
for ent in scene['entities'][:5]:
    label = ent.get('_label', '?')
    tf = ent['components']['TransformComponent']
    pos = tf['position']
    scl = tf['scale']
    print(f"  {label}: pos=({pos[0]:.1f},{pos[1]:.1f},{pos[2]:.1f}) scale=({scl[0]},{scl[1]},{scl[2]})")
