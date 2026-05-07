#!/usr/bin/env python3
"""
kf_to_gltf.py — KF_Framework 二进制格式 → glTF 2.0 (.glb) 转换器

支持的格式:
  .mesh   — 静态网格 (Vertex3d)
  .skin   — 蒙皮网格 (Vertex3dSkin)
  .model  — 节点树 (引用 .mesh / .skin 文件)
  .motion — 骨骼动画

坐标系转换: DX9 左手系 → OpenGL 右手系 (Z 轴翻转)

用法:
    # 转换单个 actor (model + 所有 motion)
    python tools/kf_to_gltf.py --actor knight --raw-dir assets/raw --out-dir assets/gltf

    # 转换单个 .mesh
    python tools/kf_to_gltf.py --mesh assets/raw/mesh/field_0.mesh --out-dir assets/gltf
"""

import argparse
import json
import math
import struct
import sys
from pathlib import Path
from typing import List, Tuple, Optional, Dict, Any

# ============================================================================
#  常量
# ============================================================================

# glTF 常量
GLTF_BYTE           = 5120
GLTF_UNSIGNED_BYTE  = 5121
GLTF_SHORT          = 5122
GLTF_UNSIGNED_SHORT = 5123
GLTF_UNSIGNED_INT   = 5125
GLTF_FLOAT          = 5126

GLTF_ARRAY_BUFFER         = 34962
GLTF_ELEMENT_ARRAY_BUFFER = 34963

GLTF_TRIANGLES = 4

# KF DrawType enum (DX9 版)
KF_TRIANGLE_LIST  = 4
KF_TRIANGLE_STRIP = 5

# KF MeshType enum
KF_MESH_3D   = 0
KF_MESH_SKIN = 1

# Vertex sizes (bytes)
VERTEX3D_SIZE     = 48   # pos:12 + norm:12 + uv:8 + color:16
VERTEX3DSKIN_SIZE = 116  # pos:12 + norm:12 + tan:12 + binorm:12 + uv:8 + 5×Short2:20 + 5×Vec2:40


# ============================================================================
#  二进制读取工具
# ============================================================================

class BinaryReader:
    def __init__(self, data: bytes):
        self.data = data
        self.pos = 0

    def read_bytes(self, n: int) -> bytes:
        result = self.data[self.pos:self.pos + n]
        self.pos += n
        return result

    def read_int32(self) -> int:
        val = struct.unpack_from('<i', self.data, self.pos)[0]
        self.pos += 4
        return val

    def read_uint16(self) -> int:
        val = struct.unpack_from('<H', self.data, self.pos)[0]
        self.pos += 2
        return val

    def read_float(self) -> float:
        val = struct.unpack_from('<f', self.data, self.pos)[0]
        self.pos += 4
        return val

    def read_vec2(self) -> Tuple[float, float]:
        return (self.read_float(), self.read_float())

    def read_vec3(self) -> Tuple[float, float, float]:
        return (self.read_float(), self.read_float(), self.read_float())

    def read_quat(self) -> Tuple[float, float, float, float]:
        x = self.read_float()
        y = self.read_float()
        z = self.read_float()
        w = self.read_float()
        return (x, y, z, w)

    def read_short2(self) -> Tuple[int, int]:
        x = struct.unpack_from('<h', self.data, self.pos)[0]
        self.pos += 2
        y = struct.unpack_from('<h', self.data, self.pos)[0]
        self.pos += 2
        return (x, y)

    def read_bool(self) -> bool:
        val = self.data[self.pos]
        self.pos += 1
        return val != 0

    def read_string(self) -> str:
        length = self.read_int32()
        s = self.data[self.pos:self.pos + length].decode('ascii', errors='replace')
        self.pos += length
        return s

    def remaining(self) -> int:
        return len(self.data) - self.pos


# ============================================================================
#  坐标系转换: DX9 左手系 → OpenGL 右手系
# ============================================================================

def flip_pos(v):
    """位置/法线/切线 Z 轴翻转"""
    return (v[0], v[1], -v[2])

def flip_quat(q):
    """四元数 (x,y,z,w) 左手→右手: 翻转 x, y"""
    return (-q[0], -q[1], q[2], q[3])

def flip_indices(indices):
    """三角形绕序反转"""
    result = []
    for i in range(0, len(indices), 3):
        if i + 2 < len(indices):
            result.extend([indices[i], indices[i+2], indices[i+1]])
        else:
            result.extend(indices[i:])
    return result


# ============================================================================
#  KF .mesh 读取
# ============================================================================

def read_mesh_file(path: Path):
    """读取 .mesh 文件, 返回 (positions, normals, uvs, colors, indices)"""
    data = path.read_bytes()
    r = BinaryReader(data)

    draw_type = r.read_int32()
    vertex_count = r.read_int32()
    index_count = r.read_int32()
    polygon_count = r.read_int32()

    positions = []
    normals = []
    uvs = []
    colors = []

    for _ in range(vertex_count):
        pos = flip_pos(r.read_vec3())
        norm = flip_pos(r.read_vec3())
        uv = r.read_vec2()
        color = (r.read_float(), r.read_float(), r.read_float(), r.read_float())
        positions.append(pos)
        normals.append(norm)
        uvs.append(uv)
        colors.append(color)

    indices = [r.read_uint16() for _ in range(index_count)]
    indices = flip_indices(indices)

    return positions, normals, uvs, colors, indices


# ============================================================================
#  KF .skin 读取
# ============================================================================

def read_skin_file(path: Path):
    """读取 .skin 文件, 返回 (positions, normals, tangents, uvs, joint_indices, joint_weights, indices)
    
    joint_indices: list of (j0,j1,j2,j3) — 取前 4 个 bone index
    joint_weights: list of (w0,w1,w2,w3) — 取前 4 个 bone weight, 归一化
    """
    data = path.read_bytes()
    r = BinaryReader(data)

    draw_type = r.read_int32()
    vertex_count = r.read_int32()
    index_count = r.read_int32()
    polygon_count = r.read_int32()

    positions = []
    normals = []
    tangents = []
    uvs = []
    joint_indices = []
    joint_weights = []

    for _ in range(vertex_count):
        pos = flip_pos(r.read_vec3())
        norm = flip_pos(r.read_vec3())
        tan = flip_pos(r.read_vec3())
        binorm = flip_pos(r.read_vec3())  # 读取但不直接使用
        uv = r.read_vec2()

        # 5 × Short2 bone indices
        bi = []
        for __ in range(5):
            s2 = r.read_short2()
            bi.extend([s2[0], s2[1]])
        # 5 × Vec2 bone weights
        bw = []
        for __ in range(5):
            v2 = r.read_vec2()
            bw.extend([v2[0], v2[1]])

        # glTF 只支持 4 个 joint influence, 取权重最大的 4 个
        pairs = list(zip(bi, bw))
        pairs.sort(key=lambda p: -p[1])
        top4 = pairs[:4]

        ji = tuple(max(0, p[0]) for p in top4)
        jw_raw = [max(0.0, p[1]) for p in top4]
        w_sum = sum(jw_raw)
        if w_sum > 0:
            jw = tuple(w / w_sum for w in jw_raw)
        else:
            jw = (1.0, 0.0, 0.0, 0.0)

        positions.append(pos)
        normals.append(norm)
        tangents.append((tan[0], tan[1], tan[2], 1.0))  # glTF tangent 是 vec4
        uvs.append(uv)
        joint_indices.append(ji)
        joint_weights.append(jw)

    indices = [r.read_uint16() for _ in range(index_count)]
    indices = flip_indices(indices)

    return positions, normals, tangents, uvs, joint_indices, joint_weights, indices


# ============================================================================
#  KF .model 读取 (递归节点树)
# ============================================================================

class ModelNode:
    def __init__(self):
        self.name = ""
        self.position = (0.0, 0.0, 0.0)
        self.rotation = (0.0, 0.0, 0.0, 1.0)  # quat xyzw
        self.scale = (1.0, 1.0, 1.0)
        self.colliders = []
        self.meshes = []   # list of dict: {mesh_name, material_name, mesh_type, ...}
        self.children = []


def read_model_node(r: BinaryReader) -> ModelNode:
    """递归读取 .model 节点"""
    node = ModelNode()

    # 名称
    node.name = r.read_string()

    # 变换
    pos = r.read_vec3()
    rot = r.read_quat()
    scl = r.read_vec3()
    node.position = flip_pos(pos)
    node.rotation = flip_quat(rot)
    node.scale = scl  # scale 不需要翻转

    # 碰撞体
    collider_count = r.read_int32()
    for _ in range(collider_count):
        col_type = r.read_int32()
        col_pos = r.read_vec3()
        col_rot = r.read_vec3()  # euler angles
        col_scale = r.read_vec3()
        col_trigger = r.read_bool()
        node.colliders.append({
            'type': col_type,
            'position': col_pos,
            'rotation': col_rot,
            'scale': col_scale,
            'is_trigger': col_trigger
        })

    # 网格引用
    mesh_count = r.read_int32()
    for _ in range(mesh_count):
        mesh_name = r.read_string()
        material_name = r.read_string()
        render_priority = r.read_int32()
        shader_type = r.read_int32()
        cast_shadow = r.read_bool()
        bs_pos = r.read_vec3()
        bs_radius = r.read_float()
        mesh_type = r.read_int32()

        node.meshes.append({
            'mesh_name': mesh_name,
            'material_name': material_name,
            'render_priority': render_priority,
            'shader_type': shader_type,
            'cast_shadow': cast_shadow,
            'bounding_sphere_pos': bs_pos,
            'bounding_sphere_radius': bs_radius,
            'mesh_type': mesh_type,  # 0=k3dMesh, 1=k3dSkin
        })

    # 子节点
    child_count = r.read_int32()
    for _ in range(child_count):
        child = read_model_node(r)
        node.children.append(child)

    return node


def read_model_file(path: Path) -> ModelNode:
    data = path.read_bytes()
    r = BinaryReader(data)
    return read_model_node(r)


# ============================================================================
#  KF .motion 读取
# ============================================================================

def read_motion_file(path: Path):
    """读取 .motion 文件, 返回 (is_loop, frames)
    frames: list of list of (translation, rotation, scale)
    """
    data = path.read_bytes()
    r = BinaryReader(data)

    is_loop = r.read_bool()
    frame_count = r.read_int32()

    frames = []
    for _ in range(frame_count):
        bone_count = r.read_int32()
        bone_transforms = []
        for __ in range(bone_count):
            t = flip_pos(r.read_vec3())
            rot = flip_quat(r.read_quat())
            s = r.read_vec3()
            bone_transforms.append((t, rot, s))
        frames.append(bone_transforms)

    return is_loop, frames


# ============================================================================
#  glTF / GLB 写入工具
# ============================================================================

class GltfBuilder:
    """构建 glTF 2.0 JSON + 二进制 buffer, 输出 .glb"""

    def __init__(self):
        self.gltf = {
            "asset": {"version": "2.0", "generator": "kf_to_gltf.py"},
            "scene": 0,
            "scenes": [{"nodes": []}],
            "nodes": [],
            "meshes": [],
            "accessors": [],
            "bufferViews": [],
            "buffers": [],
            "materials": [{
                "name": "default",
                "pbrMetallicRoughness": {
                    "baseColorFactor": [0.8, 0.8, 0.8, 1.0],
                    "metallicFactor": 0.0,
                    "roughnessFactor": 0.5
                }
            }],
        }
        self.bin_data = bytearray()

    # --- buffer helpers ---
    def _pad_to_4(self):
        remainder = len(self.bin_data) % 4
        if remainder:
            self.bin_data.extend(b'\x00' * (4 - remainder))

    def add_buffer_view(self, data: bytes, target: int = 0) -> int:
        self._pad_to_4()
        offset = len(self.bin_data)
        self.bin_data.extend(data)
        bv = {
            "buffer": 0,
            "byteOffset": offset,
            "byteLength": len(data),
        }
        if target:
            bv["target"] = target
        idx = len(self.gltf["bufferViews"])
        self.gltf["bufferViews"].append(bv)
        return idx

    def add_accessor(self, buffer_view: int, component_type: int,
                     count: int, acc_type: str,
                     min_vals=None, max_vals=None) -> int:
        acc = {
            "bufferView": buffer_view,
            "byteOffset": 0,
            "componentType": component_type,
            "count": count,
            "type": acc_type,
        }
        if min_vals is not None:
            acc["min"] = min_vals
        if max_vals is not None:
            acc["max"] = max_vals
        idx = len(self.gltf["accessors"])
        self.gltf["accessors"].append(acc)
        return idx

    # --- 顶点属性打包 ---
    def pack_vec3_list(self, vecs, target=GLTF_ARRAY_BUFFER):
        buf = bytearray()
        mins = [1e30, 1e30, 1e30]
        maxs = [-1e30, -1e30, -1e30]
        for v in vecs:
            buf.extend(struct.pack('<3f', *v))
            for i in range(3):
                mins[i] = min(mins[i], v[i])
                maxs[i] = max(maxs[i], v[i])
        bv = self.add_buffer_view(bytes(buf), target)
        return self.add_accessor(bv, GLTF_FLOAT, len(vecs), "VEC3", mins, maxs)

    def pack_vec2_list(self, vecs, target=GLTF_ARRAY_BUFFER):
        buf = bytearray()
        for v in vecs:
            buf.extend(struct.pack('<2f', *v))
        bv = self.add_buffer_view(bytes(buf), target)
        return self.add_accessor(bv, GLTF_FLOAT, len(vecs), "VEC2")

    def pack_vec4_list(self, vecs, target=GLTF_ARRAY_BUFFER):
        buf = bytearray()
        for v in vecs:
            buf.extend(struct.pack('<4f', *v))
        bv = self.add_buffer_view(bytes(buf), target)
        return self.add_accessor(bv, GLTF_FLOAT, len(vecs), "VEC4")

    def pack_uvec4_u16_list(self, vecs, target=GLTF_ARRAY_BUFFER):
        """打包 joint indices 为 unsigned short vec4"""
        buf = bytearray()
        for v in vecs:
            buf.extend(struct.pack('<4H', *[max(0, x) for x in v]))
        bv = self.add_buffer_view(bytes(buf), target)
        return self.add_accessor(bv, GLTF_UNSIGNED_SHORT, len(vecs), "VEC4")

    def pack_indices(self, indices):
        max_idx = max(indices) if indices else 0
        if max_idx <= 65535:
            buf = struct.pack(f'<{len(indices)}H', *indices)
            bv = self.add_buffer_view(buf, GLTF_ELEMENT_ARRAY_BUFFER)
            return self.add_accessor(bv, GLTF_UNSIGNED_SHORT, len(indices), "SCALAR")
        else:
            buf = struct.pack(f'<{len(indices)}I', *indices)
            bv = self.add_buffer_view(buf, GLTF_ELEMENT_ARRAY_BUFFER)
            return self.add_accessor(bv, GLTF_UNSIGNED_INT, len(indices), "SCALAR")

    def pack_scalar_float_list(self, vals):
        buf = struct.pack(f'<{len(vals)}f', *vals)
        bv = self.add_buffer_view(buf)
        min_v = min(vals) if vals else 0.0
        max_v = max(vals) if vals else 0.0
        return self.add_accessor(bv, GLTF_FLOAT, len(vals), "SCALAR",
                                 [min_v], [max_v])

    def pack_mat4_list(self, mats):
        """打包 4×4 矩阵列表 (列主序 16 floats each)"""
        buf = bytearray()
        for m in mats:
            buf.extend(struct.pack('<16f', *m))
        bv = self.add_buffer_view(bytes(buf))
        return self.add_accessor(bv, GLTF_FLOAT, len(mats), "MAT4")

    def pack_quat_list(self, quats):
        """打包四元数列表 (glTF 格式 x,y,z,w)"""
        buf = bytearray()
        for q in quats:
            buf.extend(struct.pack('<4f', q[0], q[1], q[2], q[3]))
        bv = self.add_buffer_view(bytes(buf))
        return self.add_accessor(bv, GLTF_FLOAT, len(quats), "VEC4")

    # --- 添加网格 ---
    def add_mesh_primitive(self, positions, normals, uvs, indices,
                           tangents=None, colors=None,
                           joint_indices=None, joint_weights=None,
                           name="mesh") -> int:
        """添加一个 mesh, 返回 mesh 索引"""
        attrs = {}
        attrs["POSITION"] = self.pack_vec3_list(positions)
        if normals:
            attrs["NORMAL"] = self.pack_vec3_list(normals)
        if uvs:
            attrs["TEXCOORD_0"] = self.pack_vec2_list(uvs)
        if tangents:
            attrs["TANGENT"] = self.pack_vec4_list(tangents)
        if colors:
            attrs["COLOR_0"] = self.pack_vec4_list(colors)
        if joint_indices:
            attrs["JOINTS_0"] = self.pack_uvec4_u16_list(joint_indices)
        if joint_weights:
            attrs["WEIGHTS_0"] = self.pack_vec4_list(joint_weights)

        prim = {
            "attributes": attrs,
            "indices": self.pack_indices(indices),
            "mode": GLTF_TRIANGLES,
            "material": 0,
        }
        mesh = {"name": name, "primitives": [prim]}
        idx = len(self.gltf["meshes"])
        self.gltf["meshes"].append(mesh)
        return idx

    # --- 添加节点 ---
    def add_node(self, name="", mesh_idx=None, skin_idx=None,
                 translation=None, rotation=None, scale=None,
                 children=None) -> int:
        node = {"name": name}
        if mesh_idx is not None:
            node["mesh"] = mesh_idx
        if skin_idx is not None:
            node["skin"] = skin_idx
        if translation and translation != (0, 0, 0):
            node["translation"] = list(translation)
        if rotation and rotation != (0, 0, 0, 1):
            # glTF: [x, y, z, w]
            node["rotation"] = list(rotation)
        if scale and scale != (1, 1, 1):
            node["scale"] = list(scale)
        if children:
            node["children"] = children
        idx = len(self.gltf["nodes"])
        self.gltf["nodes"].append(node)
        return idx

    # --- 导出 GLB ---
    def write_glb(self, path: Path):
        # 完成 buffer
        self._pad_to_4()
        self.gltf["buffers"] = [{"byteLength": len(self.bin_data)}]

        # JSON chunk
        json_str = json.dumps(self.gltf, separators=(',', ':'))
        json_bytes = json_str.encode('utf-8')
        # pad to 4 bytes with spaces
        json_pad = (4 - len(json_bytes) % 4) % 4
        json_bytes += b' ' * json_pad

        # GLB header
        total_length = 12 + 8 + len(json_bytes) + 8 + len(self.bin_data)
        header = struct.pack('<III', 0x46546C67, 2, total_length)
        json_chunk_header = struct.pack('<II', len(json_bytes), 0x4E4F534A)
        bin_chunk_header = struct.pack('<II', len(self.bin_data), 0x004E4942)

        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, 'wb') as f:
            f.write(header)
            f.write(json_chunk_header)
            f.write(json_bytes)
            f.write(bin_chunk_header)
            f.write(self.bin_data)

        print(f"  [glb] 写入 {path} ({total_length} bytes)")


# ============================================================================
#  转换: 单独 .mesh → .glb (静态网格)
# ============================================================================

def convert_mesh_to_glb(mesh_path: Path, out_path: Path):
    print(f"[convert] .mesh → .glb: {mesh_path.name}")
    positions, normals, uvs, colors, indices = read_mesh_file(mesh_path)

    gb = GltfBuilder()
    mesh_idx = gb.add_mesh_primitive(positions, normals, uvs, indices,
                                     colors=colors,
                                     name=mesh_path.stem)
    node_idx = gb.add_node(name=mesh_path.stem, mesh_idx=mesh_idx)
    gb.gltf["scenes"][0]["nodes"].append(node_idx)
    gb.write_glb(out_path)


# ============================================================================
#  转换: actor .model → .glb (带骨骼的角色模型)
# ============================================================================

def _flatten_model_tree(node: ModelNode, parent_idx: int,
                        flat_nodes: list, raw_dir: Path):
    """将 ModelNode 树扁平化为节点列表, 同时收集 mesh 数据"""
    my_idx = len(flat_nodes)
    flat_nodes.append({
        'node': node,
        'parent_idx': parent_idx,
        'children_indices': [],
        'mesh_data': None,
    })

    # 若该节点有 mesh 引用, 读取第一个 (通常只有一个)
    if node.meshes:
        mi = node.meshes[0]
        mesh_type = mi['mesh_type']
        mesh_name = mi['mesh_name']
        if mesh_type == KF_MESH_3D:
            mesh_file = raw_dir / "mesh" / f"{mesh_name}.mesh"
            if mesh_file.exists():
                flat_nodes[my_idx]['mesh_data'] = {
                    'type': 'mesh',
                    'data': read_mesh_file(mesh_file),
                    'name': mesh_name,
                }
        elif mesh_type == KF_MESH_SKIN:
            skin_file = raw_dir / "skin" / f"{mesh_name}.skin"
            if skin_file.exists():
                flat_nodes[my_idx]['mesh_data'] = {
                    'type': 'skin',
                    'data': read_skin_file(skin_file),
                    'name': mesh_name,
                }

    for child in node.children:
        child_idx = _flatten_model_tree(child, my_idx, flat_nodes, raw_dir)
        flat_nodes[my_idx]['children_indices'].append(child_idx)

    return my_idx


def _compute_inverse_bind_matrix(flat_nodes, bone_idx):
    """通过递归向上遍历父节点, 计算 inverse bind matrix (列主序)"""
    # 先构建 world transform = parent_world * local_TRS
    chain = []
    idx = bone_idx
    while idx >= 0:
        chain.append(idx)
        idx = flat_nodes[idx]['parent_idx']
    chain.reverse()

    # 构建 world matrix
    world = _identity_mat4()
    for i in chain:
        n = flat_nodes[i]['node']
        local = _trs_to_mat4(n.position, n.rotation, n.scale)
        world = _mat4_mul(world, local)

    # inverse bind matrix = inverse(world)
    inv = _mat4_inverse(world)
    return inv


def _identity_mat4():
    return [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]


def _trs_to_mat4(t, r, s):
    """TRS → 列主序 4×4 矩阵 (glTF convention)"""
    # rotation quaternion to matrix
    x, y, z, w = r
    xx = x*x; yy = y*y; zz = z*z
    xy = x*y; xz = x*z; yz = y*z
    wx = w*x; wy = w*y; wz = w*z

    rm = [
        1 - 2*(yy+zz), 2*(xy+wz),     2*(xz-wy),     0,
        2*(xy-wz),     1 - 2*(xx+zz), 2*(yz+wx),     0,
        2*(xz+wy),     2*(yz-wx),     1 - 2*(xx+yy), 0,
        0, 0, 0, 1
    ]

    # apply scale
    rm[0] *= s[0]; rm[1] *= s[0]; rm[2] *= s[0]
    rm[4] *= s[1]; rm[5] *= s[1]; rm[6] *= s[1]
    rm[8] *= s[2]; rm[9] *= s[2]; rm[10] *= s[2]

    # apply translation
    rm[12] = t[0]
    rm[13] = t[1]
    rm[14] = t[2]

    return rm


def _mat4_mul(a, b):
    """列主序 4×4 矩阵乘法"""
    result = [0.0] * 16
    for col in range(4):
        for row in range(4):
            s = 0.0
            for k in range(4):
                s += a[k*4 + row] * b[col*4 + k]
            result[col*4 + row] = s
    return result


def _mat4_inverse(m):
    """4×4 矩阵求逆 (列主序)"""
    # Cofactor expansion
    inv = [0.0] * 16

    inv[0] = (m[5]*m[10]*m[15] - m[5]*m[11]*m[14] - m[9]*m[6]*m[15] +
              m[9]*m[7]*m[14] + m[13]*m[6]*m[11] - m[13]*m[7]*m[10])
    inv[4] = (-m[4]*m[10]*m[15] + m[4]*m[11]*m[14] + m[8]*m[6]*m[15] -
              m[8]*m[7]*m[14] - m[12]*m[6]*m[11] + m[12]*m[7]*m[10])
    inv[8] = (m[4]*m[9]*m[15] - m[4]*m[11]*m[13] - m[8]*m[5]*m[15] +
              m[8]*m[7]*m[13] + m[12]*m[5]*m[11] - m[12]*m[7]*m[9])
    inv[12] = (-m[4]*m[9]*m[14] + m[4]*m[10]*m[13] + m[8]*m[5]*m[14] -
               m[8]*m[6]*m[13] - m[12]*m[5]*m[10] + m[12]*m[6]*m[9])

    inv[1] = (-m[1]*m[10]*m[15] + m[1]*m[11]*m[14] + m[9]*m[2]*m[15] -
              m[9]*m[3]*m[14] - m[13]*m[2]*m[11] + m[13]*m[3]*m[10])
    inv[5] = (m[0]*m[10]*m[15] - m[0]*m[11]*m[14] - m[8]*m[2]*m[15] +
              m[8]*m[3]*m[14] + m[12]*m[2]*m[11] - m[12]*m[3]*m[10])
    inv[9] = (-m[0]*m[9]*m[15] + m[0]*m[11]*m[13] + m[8]*m[1]*m[15] -
              m[8]*m[3]*m[13] - m[12]*m[1]*m[11] + m[12]*m[3]*m[9])
    inv[13] = (m[0]*m[9]*m[14] - m[0]*m[10]*m[13] - m[8]*m[1]*m[14] +
               m[8]*m[2]*m[13] + m[12]*m[1]*m[10] - m[12]*m[2]*m[9])

    inv[2] = (m[1]*m[6]*m[15] - m[1]*m[7]*m[14] - m[5]*m[2]*m[15] +
              m[5]*m[3]*m[14] + m[13]*m[2]*m[7] - m[13]*m[3]*m[6])
    inv[6] = (-m[0]*m[6]*m[15] + m[0]*m[7]*m[14] + m[4]*m[2]*m[15] -
              m[4]*m[3]*m[14] - m[12]*m[2]*m[7] + m[12]*m[3]*m[6])
    inv[10] = (m[0]*m[5]*m[15] - m[0]*m[7]*m[13] - m[4]*m[1]*m[15] +
               m[4]*m[3]*m[13] + m[12]*m[1]*m[7] - m[12]*m[3]*m[5])
    inv[14] = (-m[0]*m[5]*m[14] + m[0]*m[6]*m[13] + m[4]*m[1]*m[14] -
               m[4]*m[2]*m[13] - m[12]*m[1]*m[6] + m[12]*m[2]*m[5])

    inv[3] = (-m[1]*m[6]*m[11] + m[1]*m[7]*m[10] + m[5]*m[2]*m[11] -
              m[5]*m[3]*m[10] - m[9]*m[2]*m[7] + m[9]*m[3]*m[6])
    inv[7] = (m[0]*m[6]*m[11] - m[0]*m[7]*m[10] - m[4]*m[2]*m[11] +
              m[4]*m[3]*m[10] + m[8]*m[2]*m[7] - m[8]*m[3]*m[6])
    inv[11] = (-m[0]*m[5]*m[11] + m[0]*m[7]*m[9] + m[4]*m[1]*m[11] -
               m[4]*m[3]*m[9] - m[8]*m[1]*m[7] + m[8]*m[3]*m[5])
    inv[15] = (m[0]*m[5]*m[10] - m[0]*m[6]*m[9] - m[4]*m[1]*m[10] +
               m[4]*m[2]*m[9] + m[8]*m[1]*m[6] - m[8]*m[2]*m[5])

    det = m[0]*inv[0] + m[1]*inv[4] + m[2]*inv[8] + m[3]*inv[12]
    if abs(det) < 1e-10:
        return _identity_mat4()
    det = 1.0 / det
    return [x * det for x in inv]


def convert_actor_to_glb(actor_name: str, raw_dir: Path, out_dir: Path,
                         motion_paths: List[Path] = None):
    """将一个 actor (.model + .skin + .motion) 转换为 glTF

    输出:
      - {actor_name}.glb          — 基础模型 (mesh + skeleton)
      - {actor_name}_{motion}.glb — 每个动画一个 glb
    """
    # 读取 .model
    model_path = raw_dir / "model" / "actor" / f"{actor_name}.model"
    if not model_path.exists():
        print(f"[ERROR] 找不到 model 文件: {model_path}")
        return
    print(f"[convert] actor: {actor_name}")
    root_node = read_model_file(model_path)

    # 扁平化节点树
    flat_nodes = []
    _flatten_model_tree(root_node, -1, flat_nodes, raw_dir)

    # 识别骨骼节点 (所有节点都视为 joint)
    joint_count = len(flat_nodes)
    joint_list = list(range(joint_count))  # glTF node index == bone index

    # 找到带 skin mesh 的节点
    skin_node_idx = None
    skin_mesh_data = None
    static_meshes = []

    for i, fn in enumerate(flat_nodes):
        md = fn['mesh_data']
        if md:
            if md['type'] == 'skin':
                skin_node_idx = i
                skin_mesh_data = md
            else:
                static_meshes.append((i, md))

    # ---- 构建基础 glb ----
    gb = GltfBuilder()

    # 1. 添加所有骨骼节点
    for i, fn in enumerate(flat_nodes):
        n = fn['node']
        children_gltf = fn['children_indices'] if fn['children_indices'] else None
        gb.add_node(
            name=n.name,
            translation=n.position,
            rotation=n.rotation,
            scale=n.scale,
            children=children_gltf
        )

    # 2. 场景根节点
    gb.gltf["scenes"][0]["nodes"] = [0]  # root

    # 3. 添加 skin mesh
    if skin_mesh_data:
        positions, normals, tangents, uvs, ji, jw, indices = skin_mesh_data['data']
        mesh_idx = gb.add_mesh_primitive(
            positions, normals, uvs, indices,
            tangents=tangents,
            joint_indices=ji,
            joint_weights=jw,
            name=skin_mesh_data['name']
        )

        # inverse bind matrices
        ibm_list = []
        for bone_i in range(joint_count):
            ibm = _compute_inverse_bind_matrix(flat_nodes, bone_i)
            ibm_list.append(ibm)
        ibm_acc = gb.pack_mat4_list(ibm_list)

        # skin 定义
        skin_def = {
            "inverseBindMatrices": ibm_acc,
            "joints": joint_list,
            "name": actor_name + "_skin"
        }
        if "skins" not in gb.gltf:
            gb.gltf["skins"] = []
        skin_gltf_idx = len(gb.gltf["skins"])
        gb.gltf["skins"].append(skin_def)

        # 将 mesh 附加到 skin 所在节点
        gb.gltf["nodes"][skin_node_idx]["mesh"] = mesh_idx
        gb.gltf["nodes"][skin_node_idx]["skin"] = skin_gltf_idx

    # 4. 添加静态 meshes
    for node_i, md in static_meshes:
        positions, normals, uvs, colors, indices = md['data']
        mesh_idx = gb.add_mesh_primitive(
            positions, normals, uvs, indices,
            colors=colors, name=md['name']
        )
        gb.gltf["nodes"][node_i]["mesh"] = mesh_idx

    # 写入基础 glb
    base_glb_path = out_dir / f"{actor_name}.glb"
    gb.write_glb(base_glb_path)

    # ---- 为每个 motion 生成单独的 glb ----
    if not motion_paths:
        return

    for motion_path in motion_paths:
        motion_name = motion_path.stem  # e.g. "knight_idle"
        print(f"[convert] .motion → .glb: {motion_name}")

        is_loop, frames = read_motion_file(motion_path)
        if not frames:
            print(f"  [WARN] 空动画: {motion_name}")
            continue

        bone_count_anim = len(frames[0]) if frames else 0
        frame_count = len(frames)

        # 克隆基础 glb (重新构建, 加上动画)
        gb2 = GltfBuilder()

        # 添加骨骼节点 (same as base)
        for i, fn in enumerate(flat_nodes):
            n = fn['node']
            children_gltf = fn['children_indices'] if fn['children_indices'] else None
            gb2.add_node(
                name=n.name,
                translation=n.position,
                rotation=n.rotation,
                scale=n.scale,
                children=children_gltf
            )
        gb2.gltf["scenes"][0]["nodes"] = [0]

        # 添加 skin mesh (same)
        if skin_mesh_data:
            positions, normals, tangents, uvs, ji, jw, indices = skin_mesh_data['data']
            mesh_idx = gb2.add_mesh_primitive(
                positions, normals, uvs, indices,
                tangents=tangents,
                joint_indices=ji,
                joint_weights=jw,
                name=skin_mesh_data['name']
            )
            ibm_list = []
            for bone_i in range(joint_count):
                ibm = _compute_inverse_bind_matrix(flat_nodes, bone_i)
                ibm_list.append(ibm)
            ibm_acc = gb2.pack_mat4_list(ibm_list)
            skin_def = {
                "inverseBindMatrices": ibm_acc,
                "joints": joint_list,
                "name": actor_name + "_skin"
            }
            if "skins" not in gb2.gltf:
                gb2.gltf["skins"] = []
            skin_gltf_idx = len(gb2.gltf["skins"])
            gb2.gltf["skins"].append(skin_def)
            gb2.gltf["nodes"][skin_node_idx]["mesh"] = mesh_idx
            gb2.gltf["nodes"][skin_node_idx]["skin"] = skin_gltf_idx

        # 添加动画
        # 假设 30 FPS
        fps = 30.0
        time_keys = [i / fps for i in range(frame_count)]
        time_acc = gb2.pack_scalar_float_list(time_keys)

        channels = []
        samplers = []

        # 限制 bone_count 到 flat_nodes 数量
        anim_bone_count = min(bone_count_anim, joint_count)

        for bone_i in range(anim_bone_count):
            trans_keys = []
            rot_keys = []
            scale_keys = []
            for frame in frames:
                if bone_i < len(frame):
                    t, r, s = frame[bone_i]
                else:
                    t = (0, 0, 0)
                    r = (0, 0, 0, 1)
                    s = (1, 1, 1)
                trans_keys.append(t)
                rot_keys.append(r)
                scale_keys.append(s)

            # Translation
            trans_acc = gb2.pack_vec3_list(trans_keys, target=0)
            sampler_idx = len(samplers)
            samplers.append({
                "input": time_acc,
                "output": trans_acc,
                "interpolation": "LINEAR"
            })
            channels.append({
                "sampler": sampler_idx,
                "target": {"node": bone_i, "path": "translation"}
            })

            # Rotation
            rot_acc = gb2.pack_quat_list(rot_keys)
            sampler_idx = len(samplers)
            samplers.append({
                "input": time_acc,
                "output": rot_acc,
                "interpolation": "LINEAR"
            })
            channels.append({
                "sampler": sampler_idx,
                "target": {"node": bone_i, "path": "rotation"}
            })

            # Scale
            scale_acc = gb2.pack_vec3_list(scale_keys, target=0)
            sampler_idx = len(samplers)
            samplers.append({
                "input": time_acc,
                "output": scale_acc,
                "interpolation": "LINEAR"
            })
            channels.append({
                "sampler": sampler_idx,
                "target": {"node": bone_i, "path": "scale"}
            })

        if "animations" not in gb2.gltf:
            gb2.gltf["animations"] = []
        gb2.gltf["animations"].append({
            "name": motion_name,
            "channels": channels,
            "samplers": samplers,
        })

        anim_glb_path = out_dir / f"{motion_name}.glb"
        gb2.write_glb(anim_glb_path)


# ============================================================================
#  CLI
# ============================================================================

def main():
    parser = argparse.ArgumentParser(description="KF 二进制 → glTF 2.0 转换器")
    parser.add_argument("--actor", type=str, help="Actor 名称 (e.g. knight)")
    parser.add_argument("--mesh", type=str, help="单个 .mesh 文件路径")
    parser.add_argument("--raw-dir", type=str, default=None,
                        help="assets/raw 目录路径")
    parser.add_argument("--out-dir", type=str, default=None,
                        help="glTF 输出目录")
    args = parser.parse_args()

    work_dir = Path(__file__).resolve().parent.parent
    raw_dir = Path(args.raw_dir) if args.raw_dir else work_dir / "assets" / "raw"
    out_dir = Path(args.out_dir) if args.out_dir else work_dir / "assets" / "gltf"
    out_dir.mkdir(parents=True, exist_ok=True)

    if args.mesh:
        mesh_path = Path(args.mesh)
        out_path = out_dir / (mesh_path.stem + ".glb")
        convert_mesh_to_glb(mesh_path, out_path)

    elif args.actor:
        actor_name = args.actor
        # 查找所有 motion 文件
        motion_dir = raw_dir / "motion"
        motion_paths = []
        if motion_dir.exists():
            for f in sorted(motion_dir.glob(f"{actor_name}*.motion")):
                motion_paths.append(f)
            # 也搜索不带 actor 前缀的 motion (如 idle.motion)
            # 先只用带前缀的
        print(f"[kf_to_gltf] Actor: {actor_name}")
        print(f"[kf_to_gltf] Raw 目录: {raw_dir}")
        print(f"[kf_to_gltf] 输出目录: {out_dir}")
        print(f"[kf_to_gltf] 找到 {len(motion_paths)} 个 motion 文件")
        convert_actor_to_glb(actor_name, raw_dir, out_dir, motion_paths)

    else:
        print("请指定 --actor 或 --mesh 参数")
        parser.print_help()
        sys.exit(1)

    print("[kf_to_gltf] 完成!")


if __name__ == "__main__":
    main()
