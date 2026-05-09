#!/usr/bin/env python3
"""
convert_stage_to_dse_scene.py — 将 KF demo.stage 转成 DSE 原生 JSON 场景文件

对应 KF 源码: StageSpawner::LoadEnvironment("demo")

坐标系转换: KF DX9(LH) → DSE OpenGL(RH)
  Position:   (x*100, y*100, -z*100)
  Quaternion: (-qx, -qy, qz, qw)   [flip_quat from kf_to_gltf.py]
  Scale:      不变, ×100 (模型已在 glb 导出时 ×100 缩放)
"""

import json
import math
import struct
import sys
from pathlib import Path

KF_DATA   = Path(r"C:\Users\Administrator\Desktop\temp_analysis\KF_Framework\data")
WORK_DIR  = Path(__file__).resolve().parent.parent
OUT_PATH  = WORK_DIR / "scenes" / "kf_demo_stage.json"


# ============================================================================
#  KF binary readers
# ============================================================================

class BinaryReader:
    def __init__(self, data):
        self.data = data; self.pos = 0
    def read_int32(self):
        v = struct.unpack_from('<i', self.data, self.pos)[0]; self.pos += 4; return v
    def read_float(self):
        v = struct.unpack_from('<f', self.data, self.pos)[0]; self.pos += 4; return v
    def read_bool(self):
        v = self.data[self.pos]; self.pos += 1; return bool(v)
    def read_vec3(self):
        x, y, z = struct.unpack_from('<3f', self.data, self.pos); self.pos += 12
        return (x, y, z)
    def read_quat(self):
        x, y, z, w = struct.unpack_from('<4f', self.data, self.pos); self.pos += 16
        return (x, y, z, w)
    def read_string(self):
        length = struct.unpack_from('<i', self.data, self.pos)[0]; self.pos += 4
        s = self.data[self.pos:self.pos+length].decode('ascii', errors='replace')
        self.pos += length
        return s


def parse_model_file(model_path):
    """Parse a .model file, return list of {mesh, material, node_name}"""
    data = model_path.read_bytes()
    r = BinaryReader(data)
    meshes = []
    _parse_node(r, meshes)
    return meshes

def _parse_node(r, meshes):
    name = r.read_string()
    r.read_vec3(); r.read_quat(); r.read_vec3()  # pos, rot, scale
    for _ in range(r.read_int32()):  # colliders
        r.read_int32(); r.read_vec3(); r.read_vec3(); r.read_vec3(); r.read_bool()
    for _ in range(r.read_int32()):  # meshes
        mesh_name = r.read_string()
        mat_name  = r.read_string()
        r.read_int32(); r.read_int32(); r.read_bool()
        r.read_vec3(); r.read_float(); r.read_int32()
        meshes.append({'mesh': mesh_name, 'material': mat_name, 'node': name})
    for _ in range(r.read_int32()):  # children
        _parse_node(r, meshes)


# ============================================================================
#  KF → DSE coordinate conversion
# ============================================================================

def flip_pos(kf):
    """KF pos → DSE pos (×100, z flip)"""
    return [kf[0] * 100.0, kf[1] * 100.0, -kf[2] * 100.0]

def flip_quat(kf_q):
    """KF quat(x,y,z,w) → DSE quat(x,y,z,w): negate x,y"""
    return [-kf_q[0], -kf_q[1], kf_q[2], kf_q[3]]


# ============================================================================
#  Texture lookup
# ============================================================================

TEX_DIR = KF_DATA / "texture"
DSE_TEX_DIR = WORK_DIR / "assets" / "textures" / "stage"

def find_texture_path(mat_name):
    """Find texture for a KF material name, return DSE relative path or None"""
    for ext in ('.jpg', '.png', '.tga', '.bmp', '.jpeg'):
        p = TEX_DIR / (mat_name + ext)
        if p.exists():
            return f"assets/textures/stage/{p.name}"
    # strip trailing _0 etc
    base = mat_name.rsplit('_', 1)[0] if '_' in mat_name else mat_name
    for ext in ('.jpg', '.png', '.tga', '.bmp'):
        p = TEX_DIR / (base + ext)
        if p.exists():
            return f"assets/textures/stage/{p.name}"
    return None


# ============================================================================
#  Parse demo.stage
# ============================================================================

def parse_stage(stage_path):
    data = stage_path.read_bytes()
    off = 0
    count = struct.unpack_from('<I', data, off)[0]; off += 4
    models = []
    for _ in range(count):
        name_size = struct.unpack_from('<I', data, off)[0]; off += 4
        name = data[off:off+name_size].decode('ascii', errors='replace'); off += name_size
        inst_count = struct.unpack_from('<I', data, off)[0]; off += 4
        instances = []
        for _ in range(inst_count):
            px, py, pz = struct.unpack_from('<3f', data, off); off += 12
            qx, qy, qz, qw = struct.unpack_from('<4f', data, off); off += 16
            sx, sy, sz = struct.unpack_from('<3f', data, off); off += 12
            instances.append({
                'pos': (px, py, pz), 'quat': (qx, qy, qz, qw), 'scale': (sx, sy, sz)
            })
        models.append({'name': name, 'instances': instances})
    return models


# ============================================================================
#  Generate DSE JSON scene
# ============================================================================

def main():
    stage_path = KF_DATA / "stage" / "demo.stage"
    models = parse_stage(stage_path)
    active = [m for m in models if m['instances']]

    print(f"=== demo.stage: {len(active)} active model types ===")

    entities = []
    entity_id = 1
    warnings = []

    for model_info in active:
        model_name = model_info['name']
        model_path = KF_DATA / "model" / model_name
        if not model_path.exists():
            warnings.append(f"[SKIP] Model file not found: {model_name}")
            continue

        mesh_refs = parse_model_file(model_path)
        if not mesh_refs:
            warnings.append(f"[SKIP] No meshes in model: {model_name}")
            continue

        # Check .dmesh availability
        cooked_dir = WORK_DIR / "cooked"
        mesh_dmesh_map = {}
        for ref in mesh_refs:
            dmesh_name = ref['mesh'] + ".dmesh"
            dmesh_path = cooked_dir / dmesh_name
            if dmesh_path.exists():
                mesh_dmesh_map[ref['mesh']] = f"cooked/{dmesh_name}"
            else:
                warnings.append(f"[WARN] Missing .dmesh: {dmesh_name} (from {model_name})")

        if not mesh_dmesh_map:
            warnings.append(f"[SKIP] No cooked meshes for: {model_name}")
            continue

        # For each instance
        for inst_idx, inst in enumerate(model_info['instances']):
            dse_pos  = flip_pos(inst['pos'])
            dse_quat = flip_quat(inst['quat'])
            dse_scale = [inst['scale'][0] * 100.0,
                         inst['scale'][1] * 100.0,
                         inst['scale'][2] * 100.0]

            # Create one entity per mesh sub-part
            for ref in mesh_refs:
                if ref['mesh'] not in mesh_dmesh_map:
                    continue

                dmesh_path = mesh_dmesh_map[ref['mesh']]
                tex_path = find_texture_path(ref['material'])

                entity = {
                    "id": entity_id,
                    "components": {
                        "TransformComponent": {
                            "position": dse_pos,
                            "rotation": dse_quat,
                            "scale": dse_scale
                        },
                        "MeshRendererComponent": {
                            "mesh_path": dmesh_path,
                            "shader_variant": "MESH_HALFLAMBERT_STATIC",
                            "color": [1.0, 1.0, 1.0, 1.0],
                            "metallic": 0.0,
                            "roughness": 1.0,
                            "ao": 1.0,
                            "receive_shadow": True,
                            "visible": True
                        }
                    }
                }

                # Store texture info as comment (texture handles are runtime, loaded by Lua)
                if tex_path:
                    entity["components"]["MeshRendererComponent"]["_texture_hint"] = tex_path

                # Label for debugging
                label = model_name.replace('.model', '')
                if len(model_info['instances']) > 1:
                    label += f"_{inst_idx}"
                if len(mesh_refs) > 1:
                    label += f"_{ref['mesh']}"
                entity["_label"] = label

                entities.append(entity)
                entity_id += 1

        print(f"  {model_name}: {len(model_info['instances'])} instances, "
              f"{len(mesh_refs)} mesh parts → {len(model_info['instances']) * len([r for r in mesh_refs if r['mesh'] in mesh_dmesh_map])} entities")

    # Build scene JSON
    scene_json = {
        "name": "kf_demo_stage",
        "material_schema_version": 2,
        "entities": entities
    }

    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    with open(OUT_PATH, 'w') as f:
        json.dump(scene_json, f, indent=2)

    print(f"\n=== Output ===")
    print(f"  Scene file: {OUT_PATH}")
    print(f"  Entities: {len(entities)}")
    print(f"  File size: {OUT_PATH.stat().st_size} bytes")

    if warnings:
        print(f"\n=== Warnings ({len(warnings)}) ===")
        for w in warnings:
            print(f"  {w}")

    # Also print texture mapping for Lua
    print(f"\n=== Texture mapping (for scene.lua) ===")
    tex_map = {}
    for ent in entities:
        mc = ent["components"]["MeshRendererComponent"]
        hint = mc.get("_texture_hint")
        if hint:
            mesh_path = mc["mesh_path"]
            tex_map.setdefault(mesh_path, hint)
    for mesh_path, tex_path in sorted(tex_map.items()):
        print(f'  ["{mesh_path}"] = "{tex_path}",')


if __name__ == "__main__":
    main()
