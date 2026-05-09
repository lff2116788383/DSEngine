import struct, numpy as np

data = open('cooked/demoField.dmesh', 'rb').read()
# MeshHeader: char[4] + 4*uint32 + 3*uint64 = 4+16+24 = 44 bytes? No:
# magic(4) + version(4) + vertex_count(4) + index_count(4) + submesh_count(4) + attribute_mask(4) + 3*uint64(24) = 48
fmt = '<4sIIIII QQQ'  # Q = uint64
h = struct.unpack(fmt, data[:struct.calcsize(fmt)])
magic, version, vertex_count, index_count, submesh_count, attr_mask = h[0], h[1], h[2], h[3], h[4], h[5]
vertex_offset, index_offset, submesh_offset = h[6], h[7], h[8]
stride = 24 if version >= 2 else 20
print(f"Magic={magic} v={version} verts={vertex_count} idx={index_count} submeshes={submesh_count} attr=0x{attr_mask:X}")
print(f"vertex_offset={vertex_offset} index_offset={index_offset} submesh_offset={submesh_offset}")
print(f"stride={stride} floats ({stride*4} bytes)")

# Read vertex data
vdata_size = vertex_count * stride * 4
avail = len(data) - vertex_offset
print(f"Expected vdata={vdata_size}, available from offset={avail}")

if avail >= vdata_size:
    verts = np.frombuffer(data[vertex_offset:vertex_offset+vdata_size], dtype=np.float32).reshape(vertex_count, stride)
    print(f"\nFirst 3 vertices (stride={stride} floats):")
    for i in range(min(3, vertex_count)):
        v = verts[i]
        print(f"  V{i}: pos=({v[0]:.2f},{v[1]:.2f},{v[2]:.2f}) nrm=({v[3]:.2f},{v[4]:.2f},{v[5]:.2f}) uv=({v[6]:.4f},{v[7]:.4f})")
        if stride > 8:
            print(f"       rest[8:]: {[f'{x:.4f}' for x in v[8:]]}")
    
    # Check each 4-float block for possible vertex colors [0,1]
    for off in range(8, stride-3):
        block = verts[:, off:off+4]
        in01 = ((block >= 0) & (block <= 1.001)).all(axis=1).mean()
        if in01 > 0.8:
            avg = block.mean(axis=0)
            print(f"\n  ** Possible vertex color at offset {off}-{off+3}: avg=({avg[0]:.3f},{avg[1]:.3f},{avg[2]:.3f},{avg[3]:.3f}) [{in01:.0%} in [0,1]]")
else:
    print("NOT enough data!")
