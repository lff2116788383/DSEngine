#!/usr/bin/env python3
"""
generate_lod_meshes.py — .dmesh LOD 低模生成工具
用法:
    python tools/generate_lod_meshes.py <input.dmesh> [--ratios 0.5 0.25 0.1]
输出: <input>_lod1.dmesh, <input>_lod2.dmesh, <input>_lod3.dmesh

.dmesh 格式 (raw_scene_data.h):
  MeshHeader (40 bytes):
    char magic[4]           // "DSEM"
    uint32 version          // 1 or 2
    uint32 vertex_count
    uint32 index_count
    uint32 submesh_count
    uint32 attribute_mask
    uint64 vertex_data_offset
    uint64 index_data_offset
    uint64 submesh_data_offset

  SubMeshDesc (40 bytes each):
    uint32 index_start, index_count, base_vertex, material_id
    float[3] bounding_box_min
    float[3] bounding_box_max

  RuntimeVertex v1 (80 bytes): position(12) normal(12) texcoord(8) weights(16) joints(16) tangent(16)
  RuntimeVertex v2 (96 bytes): +color(16)
  uint32[] indices
"""

import struct
import sys
import math
import argparse
import os
from collections import defaultdict

# ---- Binary format constants ----
HEADER_FMT  = "<4sIIIIIQQQ"   # 4+4+4+4+4+4+8+8+8 = 44? let me recalc
# Actually: magic=4, version=4, vertex_count=4, index_count=4, submesh_count=4,
#           attribute_mask=4, vertex_data_offset=8, index_data_offset=8, submesh_data_offset=8
# Total: 4+4+4+4+4+4+8+8+8 = 48 bytes
HEADER_FMT  = "<4sIIIIIQQQ"  # 44 bytes? Let's count:
# 4s=4, I=4, I=4, I=4, I=4, I=4, Q=8, Q=8, Q=8 => 4+4+4+4+4+4+8+8+8 = 48 bytes
HEADER_SIZE = struct.calcsize(HEADER_FMT)  # should be 48

SUBMESH_FMT  = "<IIIIffffff"  # 4*4 + 6*4 = 40 bytes
SUBMESH_SIZE = struct.calcsize(SUBMESH_FMT)  # 40

VERTEX_V1_FMT  = "<3f3f2f4f4i4f"   # pos(12)+nor(12)+uv(8)+weights(16)+joints(16)+tangent(16) = 80
VERTEX_V1_SIZE = struct.calcsize(VERTEX_V1_FMT)  # 80
VERTEX_V2_FMT  = "<3f3f2f4f4i4f4f"  # +color(16) = 96
VERTEX_V2_SIZE = struct.calcsize(VERTEX_V2_FMT)  # 96


def read_dmesh(path):
    with open(path, "rb") as f:
        data = f.read()

    off = 0
    magic, version, vertex_count, index_count, submesh_count, attr_mask, \
        vdata_off, idata_off, sdata_off = struct.unpack_from(HEADER_FMT, data, off)
    assert magic == b"DSEM", f"Not a .dmesh file: {magic}"

    vertex_size = VERTEX_V2_SIZE if version == 2 else VERTEX_V1_SIZE
    vertex_fmt  = VERTEX_V2_FMT  if version == 2 else VERTEX_V1_FMT

    # Read submeshes
    submeshes = []
    for i in range(submesh_count):
        fields = struct.unpack_from(SUBMESH_FMT, data, sdata_off + i * SUBMESH_SIZE)
        submeshes.append({
            "index_start":   fields[0],
            "index_count":   fields[1],
            "base_vertex":   fields[2],
            "material_id":   fields[3],
            "bmin": (fields[4], fields[5], fields[6]),
            "bmax": (fields[7], fields[8], fields[9]),
        })

    # Read vertices
    vertices = []
    for i in range(vertex_count):
        v = struct.unpack_from(vertex_fmt, data, vdata_off + i * vertex_size)
        vertices.append(v)

    # Read indices
    indices = list(struct.unpack_from(f"<{index_count}I", data, idata_off))

    return {
        "version": version,
        "attr_mask": attr_mask,
        "submeshes": submeshes,
        "vertices": vertices,
        "indices": indices,
    }


def write_dmesh(path, mesh):
    version     = mesh["version"]
    attr_mask   = mesh["attr_mask"]
    submeshes   = mesh["submeshes"]
    vertices    = mesh["vertices"]
    indices     = mesh["indices"]

    vertex_fmt  = VERTEX_V2_FMT  if version == 2 else VERTEX_V1_FMT
    vertex_size = VERTEX_V2_SIZE if version == 2 else VERTEX_V1_SIZE

    submesh_count = len(submeshes)
    vertex_count  = len(vertices)
    index_count   = len(indices)

    sdata_off = HEADER_SIZE
    vdata_off = sdata_off + submesh_count * SUBMESH_SIZE
    idata_off = vdata_off + vertex_count  * vertex_size

    with open(path, "wb") as f:
        # Header
        f.write(struct.pack(HEADER_FMT,
            b"DSEM", version, vertex_count, index_count,
            submesh_count, attr_mask,
            vdata_off, idata_off, sdata_off))
        # Submeshes
        for s in submeshes:
            bmin, bmax = s["bmin"], s["bmax"]
            f.write(struct.pack(SUBMESH_FMT,
                s["index_start"], s["index_count"], s["base_vertex"], s["material_id"],
                bmin[0], bmin[1], bmin[2], bmax[0], bmax[1], bmax[2]))
        # Vertices
        for v in vertices:
            f.write(struct.pack(vertex_fmt, *v))
        # Indices
        f.write(struct.pack(f"<{index_count}I", *indices))


def compute_face_quadrics(vertices, indices):
    """Compute per-vertex quadric (4x4 symmetric matrix as 10 floats) using Quadric Error Metrics."""
    n = len(vertices)
    quadrics = [[0.0] * 10 for _ in range(n)]  # Upper triangle of 4x4 symmetric matrix

    tri_count = len(indices) // 3
    for t in range(tri_count):
        i0, i1, i2 = indices[t*3], indices[t*3+1], indices[t*3+2]
        p0 = vertices[i0][:3]
        p1 = vertices[i1][:3]
        p2 = vertices[i2][:3]

        # Compute plane equation ax+by+cz+d=0
        e1 = (p1[0]-p0[0], p1[1]-p0[1], p1[2]-p0[2])
        e2 = (p2[0]-p0[0], p2[1]-p0[1], p2[2]-p0[2])
        nx = e1[1]*e2[2] - e1[2]*e2[1]
        ny = e1[2]*e2[0] - e1[0]*e2[2]
        nz = e1[0]*e2[1] - e1[1]*e2[0]
        nlen = math.sqrt(nx*nx + ny*ny + nz*nz)
        if nlen < 1e-10:
            continue
        nx /= nlen; ny /= nlen; nz /= nlen
        d = -(nx*p0[0] + ny*p0[1] + nz*p0[2])

        # Outer product [a,b,c,d]^T [a,b,c,d]
        plane = (nx, ny, nz, d)
        kp = [plane[i]*plane[j] for i in range(4) for j in range(i, 4)]  # 10 elements

        for vi in [i0, i1, i2]:
            for k in range(10):
                quadrics[vi][k] += kp[k]

    return quadrics


def simplify_submesh(vertices, indices, target_ratio):
    """Simple vertex clustering LOD. Fast but approximate."""
    if len(indices) < 6:
        return vertices, indices

    target_triangles = max(1, int(len(indices) // 3 * target_ratio))

    # Vertex clustering: divide bounding box into grid cells
    if not vertices:
        return vertices, indices

    bmin = [min(v[j] for v in vertices) for j in range(3)]
    bmax = [max(v[j] for v in vertices) for j in range(3)]
    extent = [max(bmax[j] - bmin[j], 1e-6) for j in range(3)]

    # Number of grid cells per axis
    grid_res = max(2, int(math.cbrt(target_triangles * 4)))
    grid_res = min(grid_res, 64)

    def cell_id(v):
        cx = int((v[0] - bmin[0]) / extent[0] * (grid_res - 1))
        cy = int((v[1] - bmin[1]) / extent[1] * (grid_res - 1))
        cz = int((v[2] - bmin[2]) / extent[2] * (grid_res - 1))
        return cx + cy * grid_res + cz * grid_res * grid_res

    # Map old vertex index → cluster representative
    cluster_rep = {}     # cell_id → representative vertex index
    new_index   = {}     # old_idx → new_idx
    new_verts   = []

    for old_i, v in enumerate(vertices):
        cid = cell_id(v)
        if cid not in cluster_rep:
            cluster_rep[cid] = len(new_verts)
            new_verts.append(v)
        new_index[old_i] = cluster_rep[cid]

    # Rebuild index buffer, skip degenerate triangles
    new_indices = []
    for t in range(len(indices) // 3):
        a = new_index.get(indices[t*3],   0)
        b = new_index.get(indices[t*3+1], 0)
        c = new_index.get(indices[t*3+2], 0)
        if a != b and b != c and a != c:
            new_indices.extend([a, b, c])

    return new_verts, new_indices


def generate_lod(input_path, ratios):
    print(f"Reading: {input_path}")
    mesh = read_dmesh(input_path)
    base  = os.path.splitext(input_path)[0]

    print(f"  {len(mesh['vertices'])} vertices, {len(mesh['indices'])//3} triangles, "
          f"{len(mesh['submeshes'])} submeshes")

    for lod_idx, ratio in enumerate(ratios, start=1):
        lod_mesh = {
            "version":   mesh["version"],
            "attr_mask": mesh["attr_mask"],
            "submeshes": [],
            "vertices":  [],
            "indices":   [],
        }

        global_voff = 0
        global_ioff = 0

        for sm in mesh["submeshes"]:
            # Extract submesh vertices/indices
            base_v  = sm["base_vertex"]
            sub_vs  = mesh["vertices"][base_v : base_v + max(sm["index_start"] + sm["index_count"], 0)]

            # Remap: get the vertex range actually used by this submesh's indices
            sm_indices_global = mesh["indices"][sm["index_start"] : sm["index_start"] + sm["index_count"]]
            if not sm_indices_global:
                lod_mesh["submeshes"].append({**sm,
                    "index_start": global_ioff, "index_count": 0,
                    "base_vertex": global_voff})
                continue

            used = sorted(set(sm_indices_global))
            remap = {old: new for new, old in enumerate(used)}
            local_verts   = [mesh["vertices"][i] for i in used]
            local_indices = [remap[i] for i in sm_indices_global]

            # Simplify
            new_verts, new_indices = simplify_submesh(local_verts, local_indices, ratio)

            # Compute new bounding box
            if new_verts:
                bmin = tuple(min(v[j] for v in new_verts) for j in range(3))
                bmax = tuple(max(v[j] for v in new_verts) for j in range(3))
            else:
                bmin, bmax = sm["bmin"], sm["bmax"]

            lod_mesh["submeshes"].append({
                "index_start": global_ioff,
                "index_count": len(new_indices),
                "base_vertex": global_voff,
                "material_id": sm["material_id"],
                "bmin": bmin,
                "bmax": bmax,
            })
            lod_mesh["vertices"].extend(new_verts)
            lod_mesh["indices"].extend([i + global_voff for i in new_indices])

            global_voff += len(new_verts)
            global_ioff += len(new_indices)

        # Fix indices: they should be relative to 0 within the submesh, not global
        # Actually for dmesh, indices ARE global (base_vertex offsets the draw)
        # Re-fix: indices are relative to base_vertex=0 of the whole buffer
        # Vertices are concatenated, so indices must be absolute to the combined vertex buffer
        # The write above already uses global_voff offsets correctly.

        out_path = f"{base}_lod{lod_idx}.dmesh"
        write_dmesh(out_path, lod_mesh)

        total_verts  = len(lod_mesh["vertices"])
        total_tris   = len(lod_mesh["indices"]) // 3
        orig_tris    = len(mesh["indices"]) // 3
        actual_ratio = total_tris / max(orig_tris, 1)
        print(f"  LOD{lod_idx} ({ratio:.0%} target → {actual_ratio:.0%} actual): "
              f"{total_verts} verts, {total_tris} tris → {out_path}")


def main():
    parser = argparse.ArgumentParser(description="Generate LOD .dmesh files")
    parser.add_argument("input", help="Input .dmesh file")
    parser.add_argument("--ratios", nargs="+", type=float, default=[0.5, 0.2, 0.05],
                        help="Target reduction ratios (default: 0.5 0.2 0.05)")
    args = parser.parse_args()

    generate_lod(args.input, args.ratios)


if __name__ == "__main__":
    main()
