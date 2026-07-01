#version 450

// Virtual Geometry Visibility Buffer Resolve Fragment Shader
// Full-screen pass that reads the VisBuffer, recovers per-pixel attributes
// via barycentric interpolation, and outputs to GBuffer.
//
// Bindings:
//   0 = VisBuffer (readonly SSBO): uvec2[]
//   1 = VertexBuffer (readonly SSBO): VGVertexData[]
//   2 = ClusterBuffer (readonly SSBO): ClusterData[]
//   3 = IndexBuffer (readonly SSBO): uint[]
//   4 = MaterialBuffer (readonly SSBO): VGMaterialEntry[]

layout(location = 0) out vec4 o_albedo;
layout(location = 1) out vec4 o_normal;
layout(location = 2) out vec4 o_orm;      // occlusion, roughness, metallic

struct VGVertex {
    vec3  position;  float pad0;
    vec3  normal;    float pad1;
    vec2  uv;        vec2  pad2;
};

struct ClusterData {
    vec4  sphere;
    vec4  cone;
    uint  vertex_offset;
    uint  vertex_count;
    uint  index_offset;
    uint  index_count;
    uint  material_id;
    uint  instance_id;
    uint  pad0, pad1;
};

struct MaterialEntry {
    vec4  base_color;
    float metallic;
    float roughness;
    float ao;
    uint  albedo_tex;
    uint  normal_tex;
    uint  orm_tex;
    uint  pad0, pad1;
};

layout(set = 0, binding = 0, std430) readonly buffer VisBufferSSBO {
    uvec2 vis_buffer[];
};

layout(set = 0, binding = 1, std430) readonly buffer VertexBufferSSBO {
    VGVertex vertices[];
};

layout(set = 0, binding = 2, std430) readonly buffer ClusterBufferSSBO {
    ClusterData cluster_data[];
};

layout(set = 0, binding = 3, std430) readonly buffer IndexBufferSSBO {
    uint index_buffer[];
};

layout(set = 0, binding = 4, std430) readonly buffer MaterialBufferSSBO {
    MaterialEntry materials[];
};

layout(push_constant) uniform PC {
    mat4  u_inv_view_proj;
    vec4  u_camera_pos;
    float u_screen_width;
    float u_screen_height;
    uint  u_cluster_count;
    uint  u_material_count;
} pc;

void main() {
    ivec2 coord = ivec2(gl_FragCoord.xy);
    uint buf_idx = uint(coord.y) * uint(pc.u_screen_width) + uint(coord.x);

    uvec2 entry = vis_buffer[buf_idx];
    uint depth_bits = entry.x;
    uint payload = entry.y;

    // Empty pixel
    if (payload == 0xFFFFFFFFu) discard;

    uint cluster_id  = (payload >> 16u) & 0xFFFFu;
    uint triangle_id = (payload >> 8u) & 0xFFu;
    uint material_id = payload & 0xFFu;

    // Recover triangle vertices
    if (cluster_id >= pc.u_cluster_count) discard;

    ClusterData cluster = cluster_data[cluster_id];
    uint base_idx = cluster.index_offset + triangle_id * 3u;

    uint i0 = index_buffer[base_idx];
    uint i1 = index_buffer[base_idx + 1u];
    uint i2 = index_buffer[base_idx + 2u];

    VGVertex v0 = vertices[i0];
    VGVertex v1 = vertices[i1];
    VGVertex v2 = vertices[i2];

    // Reconstruct world position from screen coords + depth
    float ndc_x = (float(coord.x) + 0.5) / pc.u_screen_width * 2.0 - 1.0;
    float ndc_y = (float(coord.y) + 0.5) / pc.u_screen_height * 2.0 - 1.0;

    // Unsortable-uint to float depth
    uint z_bits = depth_bits;
    if ((z_bits & 0x80000000u) != 0u) {
        z_bits ^= 0x80000000u;
    } else {
        z_bits = ~z_bits;
    }
    float depth = uintBitsToFloat(z_bits);
    float ndc_z = depth * 2.0 - 1.0;

    vec4 world_pos = pc.u_inv_view_proj * vec4(ndc_x, ndc_y, ndc_z, 1.0);
    world_pos /= world_pos.w;

    // Compute barycentrics from world position
    vec3 e1 = v1.position - v0.position;
    vec3 e2 = v2.position - v0.position;
    vec3 ep = world_pos.xyz - v0.position;
    float d11 = dot(e1, e1);
    float d12 = dot(e1, e2);
    float d22 = dot(e2, e2);
    float dp1 = dot(ep, e1);
    float dp2 = dot(ep, e2);
    float denom = d11 * d22 - d12 * d12;
    float bary1 = (d22 * dp1 - d12 * dp2) / denom;
    float bary2 = (d11 * dp2 - d12 * dp1) / denom;
    float bary0 = 1.0 - bary1 - bary2;

    // Interpolate attributes
    vec3 normal = normalize(bary0 * v0.normal + bary1 * v1.normal + bary2 * v2.normal);
    vec2 uv = bary0 * v0.uv + bary1 * v1.uv + bary2 * v2.uv;

    // Read material
    MaterialEntry mat;
    if (material_id < pc.u_material_count) {
        mat = materials[material_id];
    } else {
        mat.base_color = vec4(1.0, 0.0, 1.0, 1.0);
        mat.metallic = 0.0;
        mat.roughness = 0.5;
        mat.ao = 1.0;
    }

    // Output to GBuffer
    o_albedo = mat.base_color;
    o_normal = vec4(normal * 0.5 + 0.5, 1.0);
    o_orm = vec4(mat.ao, mat.roughness, mat.metallic, 1.0);

    gl_FragDepth = depth;
}
