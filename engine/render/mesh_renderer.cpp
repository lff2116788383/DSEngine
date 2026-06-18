/**
 * @file mesh_renderer.cpp
 * @brief MeshRenderer 实现 — 见头文件说明。
 */

#include "engine/render/mesh_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"

#include <glm/gtc/matrix_inverse.hpp>

#include <cstring>
#include <vector>

namespace dse {
namespace render {

namespace {

// 与 forward_pbr.vert 输入布局一致（紧凑打包）：
// pos\@0(vec3,0) + color\@1(vec4,12) + uv\@2(vec2,28) + normal\@3(vec3,36) + tangent\@4(vec3,48)。
struct GpuMeshVertex {
    float px, py, pz;       // world-space position
    float r, g, b, a;       // vertex color
    float u, v;             // texcoord
    float nx, ny, nz;       // world-space normal
    float tx, ty, tz;       // world-space tangent
};
static_assert(sizeof(GpuMeshVertex) == 60, "GpuMeshVertex must be tightly packed (3+4+2+3+3 floats)");

// 与 forward_pbr_skinned.vert 输入布局一致（紧凑打包，92 字节）：
// pos\@0(0) + color\@1(12) + uv\@2(28) + normal\@3(36) + tangent\@4(48) +
// boneIndices\@5(60) + boneWeights\@6(76)。
struct GpuSkinnedVertex {
    float px, py, pz;       // local/bind-space position
    float r, g, b, a;       // vertex color
    float u, v;             // texcoord
    float nx, ny, nz;       // local/bind-space normal
    float tx, ty, tz;       // local/bind-space tangent
    float bi0, bi1, bi2, bi3; // bone indices
    float bw0, bw1, bw2, bw3; // bone weights
};
static_assert(sizeof(GpuSkinnedVertex) == 92, "GpuSkinnedVertex must be tightly packed (3+4+2+3+3+4+4 floats)");

// std140 PerFrame 块（176 字节）：mat4 vp + mat4 view + vec4 camera_pos +
// vec4 foliage_wind + vec4 foliage_push。着色器仅用 vp / camera_pos。
struct FwdPerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;
    glm::vec4 foliage_wind;
    glm::vec4 foliage_push;
};
static_assert(sizeof(FwdPerFrameUBO) == 176, "FwdPerFrameUBO std140 = 176 bytes");

// std140 PerScene 块（48 字节）。
struct FwdPerSceneUBO {
    glm::vec4 light_dir_and_enabled;    ///< xyz=指向光源方向(L), w=启用
    glm::vec4 light_color_and_ambient;  ///< xyz=光色, w=环境系数
    glm::vec4 light_params;             ///< x=强度
};
static_assert(sizeof(FwdPerSceneUBO) == 48, "FwdPerSceneUBO std140 = 48 bytes");

// std140 PerMaterial 块（64 字节）。
struct FwdPerMaterialUBO {
    glm::vec4 albedo;        ///< xyz=基础色, w=金属度
    glm::vec4 roughness_ao;  ///< x=粗糙, y=ao, z=法线强度, w=alpha cutoff
    glm::vec4 emissive;      ///< xyz=自发光, w=alpha test 开关
    glm::vec4 flags;         ///< x=法线贴图, y=mr 贴图, z=自发光贴图, w=遮蔽贴图
};
static_assert(sizeof(FwdPerMaterialUBO) == 64, "FwdPerMaterialUBO std140 = 64 bytes");

} // namespace

void MeshRenderer::EnsureResources(RhiDevice& device) {
    if (init_) return;

    // 不透明几何 PSO：写/测深度（Less）、背面剔除、不混合。
    PipelineStateDesc desc;
    desc.blend_enabled = false;
    desc.depth_test_enabled = true;
    desc.depth_write_enabled = true;
    desc.depth_func = CompareFunc::Less;
    desc.culling_enabled = true;
    desc.cull_face = CullFace::Back;
    pso_ = device.CreatePipelineState(desc);

    // 1x1 白纹理：缺省纹理槽回退（采样得 1.0，配合 flags 关闭对应贴图）。
    const unsigned char white[4] = {255, 255, 255, 255};
    white_tex_ = device.CreateTexture2D(1, 1, white, /*linear_filter=*/true);

    GpuBufferDesc f_desc;
    f_desc.size = sizeof(FwdPerFrameUBO);
    f_desc.usage = GpuBufferUsage::kUniform;
    f_desc.is_dynamic = true;
    per_frame_ubo_ = device.CreateGpuBuffer(f_desc, nullptr);

    GpuBufferDesc s_desc;
    s_desc.size = sizeof(FwdPerSceneUBO);
    s_desc.usage = GpuBufferUsage::kUniform;
    s_desc.is_dynamic = true;
    per_scene_ubo_ = device.CreateGpuBuffer(s_desc, nullptr);

    GpuBufferDesc m_desc;
    m_desc.size = sizeof(FwdPerMaterialUBO);
    m_desc.usage = GpuBufferUsage::kUniform;
    m_desc.is_dynamic = true;
    per_material_ubo_ = device.CreateGpuBuffer(m_desc, nullptr);

    init_ = true;
}

void MeshRenderer::EnsureVertexCapacity(RhiDevice& device, size_t vertex_bytes) {
    if (vbo_ && vbo_capacity_ >= vertex_bytes) return;
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    GpuBufferDesc vb_desc;
    vb_desc.size = vertex_bytes;
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = true;
    vbo_ = device.CreateGpuBuffer(vb_desc, nullptr);
    vbo_capacity_ = vertex_bytes;
}

void MeshRenderer::EnsureIndexCapacity(RhiDevice& device, size_t index_bytes) {
    if (ibo_ && ibo_capacity_ >= index_bytes) return;
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    GpuBufferDesc ib_desc;
    ib_desc.size = index_bytes;
    ib_desc.usage = GpuBufferUsage::kIndex;
    ib_desc.is_dynamic = true;
    ibo_ = device.CreateGpuBuffer(ib_desc, nullptr);
    ibo_capacity_ = index_bytes;
}

void MeshRenderer::EnsureBoneCapacity(RhiDevice& device, size_t bone_bytes) {
    if (bone_ssbo_ && bone_ssbo_capacity_ >= bone_bytes) return;
    if (bone_ssbo_) device.DeleteGpuBuffer(bone_ssbo_);
    GpuBufferDesc b_desc;
    b_desc.size = bone_bytes;
    b_desc.usage = GpuBufferUsage::kStorage;
    b_desc.is_dynamic = true;
    bone_ssbo_ = device.CreateGpuBuffer(b_desc, nullptr);
    bone_ssbo_capacity_ = bone_bytes;
}

void MeshRenderer::DrawSkinned(CommandBuffer& cmd, RhiDevice& device,
                               const std::vector<SkinnedMeshVertex>& vertices,
                               const std::vector<uint16_t>& indices,
                               const glm::mat4& model,
                               const std::vector<glm::mat4>& bone_matrices,
                               const glm::mat4& view,
                               const glm::mat4& proj,
                               const glm::vec3& camera_pos,
                               const MeshMaterial& material,
                               const DirectionalLight& light) {
    if (vertices.empty() || indices.empty() || bone_matrices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrSkinned);
    if (program == 0) return;  // 该后端未提供蒙皮 forward PBR 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- 顶点打包（局部/绑定空间，VS 施骨骼混合 + vp，不在 CPU 预变换） ---
    std::vector<GpuSkinnedVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const SkinnedMeshVertex& v = vertices[i];
        GpuSkinnedVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
        g.bi0 = v.bone_indices.x; g.bi1 = v.bone_indices.y;
        g.bi2 = v.bone_indices.z; g.bi3 = v.bone_indices.w;
        g.bw0 = v.bone_weights.x; g.bw1 = v.bone_weights.y;
        g.bw2 = v.bone_weights.z; g.bw3 = v.bone_weights.w;
    }

    // --- 骨骼矩阵：左乘 model 得世界空间，写入 SSBO ---
    std::vector<glm::mat4> world_bones(bone_matrices.size());
    for (size_t i = 0; i < bone_matrices.size(); ++i) {
        world_bones[i] = model * bone_matrices[i];
    }
    const size_t bone_bytes = world_bones.size() * sizeof(glm::mat4);
    EnsureBoneCapacity(device, bone_bytes);
    if (!bone_ssbo_) return;
    device.UpdateGpuBuffer(bone_ssbo_, 0, bone_bytes, world_bones.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuSkinnedVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO 填充（与静态路径同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    scene.light_params = glm::vec4(light.intensity, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdPerMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    device.UpdateGpuBuffer(per_material_ubo_, 0, sizeof(mat), &mat);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
        VertexAttr{5u, 4u, 60u},   // bone indices
        VertexAttr{6u, 4u, 76u},   // bone weights
    };

    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());     // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());     // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_ubo_.raw());  // PerMaterial @ set2.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    // 骨骼矩阵 SSBO\@slot 0（三后端通用语义：GL binding0 / Vulkan 位置0 / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::Draw(CommandBuffer& cmd, RhiDevice& device,
                        const std::vector<MeshVertex>& vertices,
                        const std::vector<uint16_t>& indices,
                        const glm::mat4& model,
                        const glm::mat4& view,
                        const glm::mat4& proj,
                        const glm::vec3& camera_pos,
                        const MeshMaterial& material,
                        const DirectionalLight& light) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbr);
    if (program == 0) return;  // 该后端未提供 forward PBR 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间 ---
    const glm::mat3 normal_matrix = glm::inverseTranspose(glm::mat3(model));
    const glm::mat3 model3 = glm::mat3(model);
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        const glm::vec3 wp = glm::vec3(model * glm::vec4(v.position, 1.0f));
        const glm::vec3 wn = glm::normalize(normal_matrix * v.normal);
        const glm::vec3 wt = model3 * v.tangent;
        GpuMeshVertex& g = gpu_verts[i];
        g.px = wp.x; g.py = wp.y; g.pz = wp.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = wn.x; g.ny = wn.y; g.nz = wn.z;
        g.tx = wt.x; g.ty = wt.y; g.tz = wt.z;
    }

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO 填充 ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);  // L = 指向光源
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    scene.light_params = glm::vec4(light.intensity, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdPerMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    device.UpdateGpuBuffer(per_material_ubo_, 0, sizeof(mat), &mat);

    // --- 纹理（缺省回退到白纹理；flat unit 0..4） ---
    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());     // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());     // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_ubo_.raw());  // PerMaterial @ set2.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::Shutdown(RhiDevice& device) {
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (per_frame_ubo_) device.DeleteGpuBuffer(per_frame_ubo_);
    if (per_scene_ubo_) device.DeleteGpuBuffer(per_scene_ubo_);
    if (per_material_ubo_) device.DeleteGpuBuffer(per_material_ubo_);
    if (bone_ssbo_) device.DeleteGpuBuffer(bone_ssbo_);
    vbo_ = ibo_ = per_frame_ubo_ = per_scene_ubo_ = per_material_ubo_ = BufferHandle{};
    bone_ssbo_ = BufferHandle{};
    vbo_capacity_ = ibo_capacity_ = bone_ssbo_capacity_ = 0;
    init_ = false;
}

} // namespace render
} // namespace dse
