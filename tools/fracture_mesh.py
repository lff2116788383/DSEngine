#!/usr/bin/env python3
"""
fracture_mesh.py — 离线 Voronoi 网格切分工具
将一个 .dmesh 或 .obj 文件切分为 N 个碎片 .dmesh，并输出 fracture descriptor JSON。

依赖：
  pip install numpy scipy trimesh

用法：
  python fracture_mesh.py input.dmesh --fragments 8 --output-dir fragments/ --seed 42

输出：
  fragments/input_frag_00.dmesh
  fragments/input_frag_01.dmesh
  ...
  fragments/input.fracture.json
"""

import argparse
import json
import os
import struct
import sys
import numpy as np

try:
    import trimesh
    from scipy.spatial import Voronoi
except ImportError:
    print("ERROR: requires 'trimesh' and 'scipy'. Install with:")
    print("  pip install numpy scipy trimesh")
    sys.exit(1)


# ─── DMESH binary format ───────────────────────────────────────────────
# MeshHeader (packed):
#   char[4]  magic = 'DSEM'
#   uint32   version
#   uint32   vertex_count
#   uint32   index_count
#   uint32   submesh_count
#   uint32   attribute_mask
#   uint64   vertex_data_offset
#   uint64   index_data_offset
#   uint64   submesh_data_offset
#
# SubMeshDesc (packed):
#   uint32   index_start
#   uint32   index_count
#   uint32   base_vertex
#   uint32   material_id
#   float[3] bounding_box_min
#   float[3] bounding_box_max
#
# Vertex layout (v1, stride=20 floats):
#   float[3] position
#   float[3] normal
#   float[2] uv
#   float[4] weights  (bone weights, 0 for static mesh)
#   float[4] joints   (bone indices as float-encoded ints, 0 for static)
#   float[4] tangent+bitangent_sign  (tangent xyz + padding)
#
# v2 (stride=24): adds float[4] vertex color at [20-23]

MESH_HEADER_FORMAT = '<4s5I3Q'  # magic(4s) + 5 uint32 + 3 uint64
MESH_HEADER_SIZE = struct.calcsize(MESH_HEADER_FORMAT)

SUBMESH_DESC_FORMAT = '<4I6f'   # 4 uint32 + 6 float
SUBMESH_DESC_SIZE = struct.calcsize(SUBMESH_DESC_FORMAT)


def load_dmesh(filepath):
    """Load a .dmesh file and return a trimesh.Trimesh object + raw vertex data."""
    with open(filepath, 'rb') as f:
        data = f.read()

    magic, version, vcount, icount, smcount, attr_mask, \
        vdata_off, idata_off, smdata_off = struct.unpack_from(MESH_HEADER_FORMAT, data, 0)

    if magic != b'DSEM':
        raise ValueError(f"Not a valid .dmesh file (magic={magic})")

    stride = 24 if version >= 2 else 20  # floats per vertex
    vertices_raw = np.frombuffer(data, dtype=np.float32,
                                 offset=vdata_off, count=vcount * stride)
    vertices_raw = vertices_raw.reshape(vcount, stride)

    positions = vertices_raw[:, 0:3]
    normals = vertices_raw[:, 3:6]
    uvs = vertices_raw[:, 6:8]

    indices = np.frombuffer(data, dtype=np.uint32, offset=idata_off, count=icount)

    # Read submesh descriptors
    submeshes = []
    for i in range(smcount):
        off = smdata_off + i * SUBMESH_DESC_SIZE
        sm = struct.unpack_from(SUBMESH_DESC_FORMAT, data, off)
        submeshes.append({
            'index_start': sm[0],
            'index_count': sm[1],
            'base_vertex': sm[2],
            'material_id': sm[3],
        })

    # Resolve all indices (base_vertex + local index)
    all_faces = []
    for sm in submeshes:
        for i in range(0, sm['index_count'], 3):
            i0 = sm['base_vertex'] + indices[sm['index_start'] + i]
            i1 = sm['base_vertex'] + indices[sm['index_start'] + i + 1]
            i2 = sm['base_vertex'] + indices[sm['index_start'] + i + 2]
            all_faces.append([i0, i1, i2])

    faces = np.array(all_faces, dtype=np.int64)
    mesh = trimesh.Trimesh(vertices=positions, faces=faces, process=False)

    return mesh, vertices_raw, version


def write_dmesh(filepath, vertices_raw, faces, version=1):
    """Write a fragment as .dmesh binary file."""
    stride = vertices_raw.shape[1] if len(vertices_raw.shape) > 1 else 20
    vcount = vertices_raw.shape[0]
    icount = faces.size  # flat index count

    # Flatten indices (single submesh, base_vertex=0)
    flat_indices = faces.flatten().astype(np.uint32)

    # Compute bounding box
    positions = vertices_raw[:, 0:3]
    bbox_min = positions.min(axis=0)
    bbox_max = positions.max(axis=0)

    # Layout: header | submesh_desc | vertex_data | index_data
    header_size = MESH_HEADER_SIZE
    submesh_offset = header_size
    submesh_size = SUBMESH_DESC_SIZE
    vertex_offset = submesh_offset + submesh_size
    vertex_data = vertices_raw.astype(np.float32).tobytes()
    index_offset = vertex_offset + len(vertex_data)
    index_data = flat_indices.tobytes()

    header = struct.pack(MESH_HEADER_FORMAT,
                         b'DSEM',
                         version,
                         vcount,
                         icount,
                         1,  # submesh_count
                         0,  # attribute_mask
                         vertex_offset,
                         index_offset,
                         submesh_offset)

    submesh = struct.pack(SUBMESH_DESC_FORMAT,
                          0,       # index_start
                          icount,  # index_count
                          0,       # base_vertex
                          0,       # material_id
                          float(bbox_min[0]), float(bbox_min[1]), float(bbox_min[2]),
                          float(bbox_max[0]), float(bbox_max[1]), float(bbox_max[2]))

    with open(filepath, 'wb') as f:
        f.write(header)
        f.write(submesh)
        f.write(vertex_data)
        f.write(index_data)


def voronoi_fracture(mesh, vertices_raw, n_fragments, seed=42):
    """
    Cut a mesh into fragments using Voronoi tessellation.
    Returns list of (fragment_vertices_raw, fragment_faces, centroid_offset).
    """
    rng = np.random.RandomState(seed)

    # Generate seed points inside the mesh bounding box
    bbox_min = mesh.bounds[0]
    bbox_max = mesh.bounds[1]
    bbox_size = bbox_max - bbox_min

    # Generate points and filter to those inside the mesh
    seeds = []
    max_attempts = n_fragments * 50
    attempts = 0
    while len(seeds) < n_fragments and attempts < max_attempts:
        batch_size = (n_fragments - len(seeds)) * 5
        candidates = rng.rand(batch_size, 3) * bbox_size + bbox_min
        # Simple containment check: use trimesh
        inside = mesh.contains(candidates)
        seeds.extend(candidates[inside].tolist())
        attempts += batch_size

    if len(seeds) < n_fragments:
        # Fallback: use all candidates even if outside
        remaining = n_fragments - len(seeds)
        extra = (rng.rand(remaining, 3) * bbox_size + bbox_min).tolist()
        seeds.extend(extra)

    seeds = np.array(seeds[:n_fragments])

    # Assign each vertex to nearest seed → fragment assignment
    positions = vertices_raw[:, 0:3]
    # Compute distance from each vertex to each seed
    # For large meshes this could be slow, but for game assets (<100K verts) it's fine
    dists = np.linalg.norm(positions[:, np.newaxis, :] - seeds[np.newaxis, :, :], axis=2)
    vertex_labels = np.argmin(dists, axis=1)

    # Build per-fragment data
    fragments = []
    for frag_id in range(n_fragments):
        # Find vertices belonging to this fragment
        vert_mask = vertex_labels == frag_id
        vert_indices = np.where(vert_mask)[0]

        if len(vert_indices) == 0:
            continue

        # Find faces where ALL three vertices belong to this fragment
        faces = mesh.faces
        face_mask = vert_mask[faces[:, 0]] & vert_mask[faces[:, 1]] & vert_mask[faces[:, 2]]
        frag_faces = faces[face_mask]

        if len(frag_faces) == 0:
            continue

        # Remap indices to local
        old_to_new = np.full(len(positions), -1, dtype=np.int64)
        used_verts = np.unique(frag_faces.flatten())
        for new_idx, old_idx in enumerate(used_verts):
            old_to_new[old_idx] = new_idx

        frag_verts_raw = vertices_raw[used_verts]
        frag_faces_local = old_to_new[frag_faces].astype(np.int64)

        # Compute centroid (center of mass of fragment vertices)
        frag_positions = frag_verts_raw[:, 0:3]
        centroid = frag_positions.mean(axis=0)

        # Shift vertices so fragment is centered at origin
        frag_verts_raw = frag_verts_raw.copy()
        frag_verts_raw[:, 0] -= centroid[0]
        frag_verts_raw[:, 1] -= centroid[1]
        frag_verts_raw[:, 2] -= centroid[2]

        # Estimate volume (using convex hull or bbox volume as approximation)
        frag_bbox = frag_positions.max(axis=0) - frag_positions.min(axis=0)
        volume = float(frag_bbox[0] * frag_bbox[1] * frag_bbox[2])
        if volume < 1e-6:
            volume = 1e-6

        fragments.append({
            'vertices_raw': frag_verts_raw,
            'faces': frag_faces_local,
            'centroid': centroid.tolist(),
            'volume': volume,
        })

    return fragments


def main():
    parser = argparse.ArgumentParser(description='Voronoi mesh fracture tool for DSEngine')
    parser.add_argument('input', help='Input mesh file (.dmesh or .obj)')
    parser.add_argument('--fragments', '-n', type=int, default=8,
                        help='Number of fragments (default: 8)')
    parser.add_argument('--output-dir', '-o', default='.',
                        help='Output directory for fragment files')
    parser.add_argument('--seed', '-s', type=int, default=42,
                        help='Random seed for Voronoi point generation')
    parser.add_argument('--prefix', default=None,
                        help='Output filename prefix (default: input filename stem)')
    args = parser.parse_args()

    input_path = args.input
    stem = os.path.splitext(os.path.basename(input_path))[0]
    prefix = args.prefix or stem
    out_dir = args.output_dir
    os.makedirs(out_dir, exist_ok=True)

    ext = os.path.splitext(input_path)[1].lower()

    if ext == '.dmesh':
        mesh, vertices_raw, version = load_dmesh(input_path)
    elif ext == '.obj':
        mesh = trimesh.load(input_path, process=False)
        # Build a simple vertex array (position + normal + uv + zeros for weights/joints/tangent)
        version = 1
        vcount = len(mesh.vertices)
        vertices_raw = np.zeros((vcount, 20), dtype=np.float32)
        vertices_raw[:, 0:3] = mesh.vertices
        if mesh.vertex_normals is not None and len(mesh.vertex_normals) == vcount:
            vertices_raw[:, 3:6] = mesh.vertex_normals
        # UV: trimesh stores in visual.uv
        if hasattr(mesh.visual, 'uv') and mesh.visual.uv is not None and len(mesh.visual.uv) == vcount:
            vertices_raw[:, 6:8] = mesh.visual.uv
    else:
        print(f"ERROR: unsupported format '{ext}'. Supported: .dmesh, .obj")
        sys.exit(1)

    print(f"Loaded: {input_path}")
    print(f"  Vertices: {len(mesh.vertices)}, Faces: {len(mesh.faces)}")
    print(f"  Fracturing into {args.fragments} fragments (seed={args.seed})...")

    fragments = voronoi_fracture(mesh, vertices_raw, args.fragments, seed=args.seed)

    print(f"  Generated {len(fragments)} non-empty fragments")

    # Write fragment dmesh files and build descriptor
    descriptor = {
        "source_mesh": os.path.basename(input_path),
        "fragments": []
    }

    for i, frag in enumerate(fragments):
        frag_filename = f"{prefix}_frag_{i:02d}.dmesh"
        frag_path = os.path.join(out_dir, frag_filename)

        write_dmesh(frag_path, frag['vertices_raw'], frag['faces'], version=version)

        descriptor['fragments'].append({
            "mesh": frag_filename,
            "offset": [round(c, 4) for c in frag['centroid']],
            "volume": round(frag['volume'], 6)
        })

        vcount = frag['vertices_raw'].shape[0]
        fcount = frag['faces'].shape[0]
        print(f"  [{i:02d}] {frag_filename}: {vcount} verts, {fcount} faces, "
              f"vol={frag['volume']:.4f}")

    # Write fracture descriptor JSON
    json_path = os.path.join(out_dir, f"{prefix}.fracture.json")
    with open(json_path, 'w') as f:
        json.dump(descriptor, f, indent=2)

    print(f"\nFracture descriptor: {json_path}")
    print("Done!")


if __name__ == '__main__':
    main()
