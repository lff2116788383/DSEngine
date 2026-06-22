/**
 * @file mesh_renderer.cpp
 * @brief MeshRenderer 实现 — 见头文件说明。
 */

#include "engine/render/mesh_renderer.h"

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/ubo_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

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

// 与 BuiltinProgram::Sprite2D 输入布局一致（紧凑 36 字节）：
// pos\@0(vec3,0) + color\@1(vec4,12) + uv\@2(vec2,28)。无光照 2D（B2b-6）专用。
struct GpuUnlit2DVertex {
    float px, py, pz;       // world-space position
    float r, g, b, a;       // vertex color
    float u, v;             // texcoord
};
static_assert(sizeof(GpuUnlit2DVertex) == 36, "GpuUnlit2DVertex must be tightly packed (3+4+2 floats)");

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

// 与 forward_shaded_skinned_instanced.vert 的 MeshSkinnedInst（std430）逐位匹配（80 字节）：
// mat4 model(64) + int bone_offset(64) + 3×int pad(68/72/76)。配 DX11 执行器 SkinnedInstGPU 同布局。
struct MeshSkinnedInstGPU {
    glm::mat4 model;        // world-space per-instance model
    int32_t bone_offset;    // 该实例骨骼调色板在密排骨骼 SSBO 中的起始下标
    int32_t pad0, pad1, pad2;
};
static_assert(sizeof(MeshSkinnedInstGPU) == 80, "MeshSkinnedInstGPU std430 = 80 bytes (mat4 + int + 3 pad)");

// ===== 阶段4-M4：MeshDrawItem → MeshRenderer 入参映射（DrawBatch 分发翻译层）=====

// BatchVertex（pos/color/uv/normal/tangent/weights/joints）→ MeshVertex（局部空间）。
inline MeshVertex BatchToMeshVertex(const BatchVertex& bv) {
    MeshVertex mv;
    mv.position = bv.pos;
    mv.color = bv.color;
    mv.uv = bv.uv;
    mv.normal = bv.normal;
    mv.tangent = bv.tangent;
    return mv;
}

// BatchVertex → SkinnedMeshVertex（joints→bone_indices、weights→bone_weights，局部/绑定空间）。
inline SkinnedMeshVertex BatchToSkinnedVertex(const BatchVertex& bv) {
    SkinnedMeshVertex sv;
    sv.position = bv.pos;
    sv.color = bv.color;
    sv.uv = bv.uv;
    sv.normal = bv.normal;
    sv.tangent = bv.tangent;
    sv.bone_indices = bv.joints;
    sv.bone_weights = bv.weights;
    return sv;
}

// MeshDrawItem 材质字段 → ShadedMaterial（与执行器 PreparePerMaterialUBO 同源；force_unlit/
// overdraw/wireframe 由 DrawShaded 内部读 GetGlobalRenderState 生效，此处不消费）。
inline ShadedMaterial BatchToShadedMaterial(const MeshDrawItem& it) {
    ShadedMaterial m;
    m.albedo = it.material_albedo;
    m.metallic = it.material_metallic;
    m.roughness = it.material_roughness;
    m.ao = it.material_ao;
    m.normal_strength = it.material_normal_strength;
    m.emissive = it.material_emissive;
    m.alpha_cutoff = it.material_alpha_cutoff;
    m.alpha_test = it.material_alpha_test;
    m.double_sided = it.material_double_sided;
    m.shading_mode = it.shading_mode;
    m.sss_strength = it.material_sss_strength;
    m.sss_tint = it.material_sss_tint;
    m.clear_coat = it.material_clear_coat;
    m.clear_coat_roughness = it.material_clear_coat_roughness;
    m.anisotropy = it.material_anisotropy;
    m.pom_height_scale = it.material_pom_height_scale;
    m.toon_shadow_color = it.toon_shadow_color;
    m.toon_shadow_threshold = it.toon_shadow_threshold;
    m.toon_shadow_softness = it.toon_shadow_softness;
    m.toon_specular_size = it.toon_specular_size;
    m.toon_specular_strength = it.toon_specular_strength;
    m.toon_rim_strength = it.toon_rim_strength;
    m.watercolor_paper_strength = it.watercolor_paper_strength;
    m.watercolor_edge_darkening = it.watercolor_edge_darkening;
    m.watercolor_color_bleed = it.watercolor_color_bleed;
    m.watercolor_pigment_density = it.watercolor_pigment_density;
    m.albedo_tex = it.texture_handle;
    m.normal_tex = it.normal_map_handle;
    m.metallic_roughness_tex = it.metallic_roughness_map_handle;
    m.emissive_tex = it.emissive_map_handle;
    m.occlusion_tex = it.occlusion_map_handle;
    m.splat_enabled = it.splat_enabled;
    m.splat_weight_map = it.splat_weight_map_handle;
    for (int i = 0; i < 4; ++i) m.splat_layers[i] = it.splat_layer_handles[i];
    m.splat_tiling = it.splat_tiling;
    m.snow_coverage = it.snow_coverage;
    m.snow_albedo = it.snow_albedo;
    m.snow_roughness = it.snow_roughness;
    m.snow_normal_threshold = it.snow_normal_threshold;
    m.snow_edge_sharpness = it.snow_edge_sharpness;
    m.wboit_mode = it.wboit_mode;
    m.receive_shadow = it.receive_shadow;
    m.shadow_strength = it.shadow_strength;
    m.foliage = it.foliage;
    return m;
}

// MeshDrawItem 光照字段 → DirectionalLight（force_unlit 由 DrawShaded 内部生效，此处仅传 enabled）。
inline DirectionalLight BatchToDirLight(const MeshDrawItem& it) {
    DirectionalLight l;
    l.direction = it.light_direction;
    l.color = it.light_color;
    l.intensity = it.light_intensity;
    l.ambient = it.ambient_intensity;
    l.enabled = it.lighting_enabled;
    return l;
}

// MeshDrawItem 点光列表 → ShadedPointLight（语义同执行器 PreparePointLightsUBO）。
inline std::vector<ShadedPointLight> BatchToPointLights(const MeshDrawItem& it) {
    std::vector<ShadedPointLight> out;
    out.reserve(it.point_lights.size());
    for (const auto& pl : it.point_lights) {
        ShadedPointLight sp;
        sp.color = pl.color;
        sp.intensity = pl.intensity;
        sp.position = pl.position;
        sp.radius = pl.radius;
        sp.cast_shadow = pl.cast_shadow;
        sp.shadow_index = pl.shadow_index;
        out.push_back(sp);
    }
    return out;
}

// MeshDrawItem 聚光灯列表 → ShadedSpotLight（语义同执行器 PrepareSpotLightsUBO）。
inline std::vector<ShadedSpotLight> BatchToSpotLights(const MeshDrawItem& it) {
    std::vector<ShadedSpotLight> out;
    out.reserve(it.spot_lights.size());
    for (const auto& sl : it.spot_lights) {
        ShadedSpotLight ss;
        ss.color = sl.color;
        ss.intensity = sl.intensity;
        ss.position = sl.position;
        ss.radius = sl.radius;
        ss.direction = sl.direction;
        ss.inner_cone = sl.inner_cone;
        ss.outer_cone = sl.outer_cone;
        ss.cast_shadow = sl.cast_shadow;
        ss.shadow_index = sl.shadow_index;
        out.push_back(ss);
    }
    return out;
}

// 把一组 BatchVertex 在 CPU 侧按骨骼调色板蒙皮到**局部空间**（bind→local，未乘 model；
// DrawGBuffer 再施 model）。供 gbuffer 模式下蒙皮/蒙皮实例项展开为静态世界几何。
inline std::vector<MeshVertex> SkinBatchToLocal(const BatchVertex* v, size_t n,
                                                const std::vector<glm::mat4>& bones) {
    std::vector<MeshVertex> out(n);
    for (size_t i = 0; i < n; ++i) {
        const BatchVertex& bv = v[i];
        glm::mat4 skin(0.0f);
        float wsum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const float w = bv.weights[k];
            if (w <= 0.0f) continue;
            const int bi = static_cast<int>(bv.joints[k]);
            if (bi >= 0 && bi < static_cast<int>(bones.size())) {
                skin += w * bones[bi];
                wsum += w;
            }
        }
        if (wsum <= 0.0f) skin = glm::mat4(1.0f);
        const glm::mat3 skin3(skin);
        MeshVertex& mv = out[i];
        mv.position = glm::vec3(skin * glm::vec4(bv.pos, 1.0f));
        mv.color = bv.color;
        mv.uv = bv.uv;
        mv.normal = glm::normalize(skin3 * bv.normal);
        mv.tangent = skin3 * bv.tangent;
    }
    return out;
}

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

// std140 PerScene 块（304 字节）。尾部 CSM 阴影字段仅 DrawShaded 填充；
// 基础 Draw/DrawSkinned/DrawInstanced 仅写前 3 个 vec4，其余置零（forward_pbr.frag 不读 → 不回归）。
struct FwdPerSceneUBO {
    glm::vec4 light_dir_and_enabled;       ///< xyz=指向光源方向(L), w=启用
    glm::vec4 light_color_and_ambient;     ///< xyz=光色, w=环境系数
    glm::vec4 light_params;                ///< x=强度, y=shadow_strength, z=receive_shadow
    glm::vec4 cascade_splits;              ///< xyz=view-space 级联分裂距离
    glm::mat4 light_space_matrices[3];     ///< 各级联 shadow-sample 矩阵
    glm::vec4 shadow_atlas_regions[3];     ///< xy=UV scale, zw=UV offset
    glm::mat4 spot_light_space_matrices[4];///< 聚光灯 light-space 矩阵（Final-Feat-8；点光用 cube 距离无需矩阵）
};
static_assert(sizeof(FwdPerSceneUBO) == 560, "FwdPerSceneUBO std140 = 560 bytes");

// std140 PerMaterial 块（64 字节）。
struct FwdPerMaterialUBO {
    glm::vec4 albedo;        ///< xyz=基础色, w=金属度
    glm::vec4 roughness_ao;  ///< x=粗糙, y=ao, z=法线强度, w=alpha cutoff
    glm::vec4 emissive;      ///< xyz=自发光, w=alpha test 开关
    glm::vec4 flags;         ///< x=法线贴图, y=mr 贴图, z=自发光贴图, w=遮蔽贴图
};
static_assert(sizeof(FwdPerMaterialUBO) == 64, "FwdPerMaterialUBO std140 = 64 bytes");

// std140 扩展 PerMaterial 块（160 字节）—— ForwardShaded 专用，须与 forward_shaded.frag 布局逐字段一致。
struct FwdShadedMaterialUBO {
    glm::vec4 albedo;        ///< xyz=基础色, w=金属度
    glm::vec4 roughness_ao;  ///< x=粗糙, y=ao, z=法线强度, w=alpha cutoff
    glm::vec4 emissive;      ///< xyz=自发光, w=alpha test 开关
    glm::vec4 flags;         ///< x=法线贴图, y=mr 贴图, z=自发光贴图, w=遮蔽贴图
    glm::vec4 mode_params;   ///< x=shading_mode, y=double_sided, z=anisotropy, w=pom_height_scale
    glm::vec4 sss;           ///< xyz=sss_tint, w=sss_strength
    glm::vec4 clearcoat;     ///< x=clear_coat, y=clear_coat_roughness
    glm::vec4 toon_shadow;   ///< xyz=toon_shadow_color, w=toon_shadow_threshold
    glm::vec4 toon_params;   ///< x=softness, y=spec_size, z=spec_strength, w=rim_strength
    glm::vec4 watercolor;    ///< x=paper, y=edge, z=bleed, w=pigment
};
static_assert(sizeof(FwdShadedMaterialUBO) == 160, "FwdShadedMaterialUBO std140 = 160 bytes");

// Final-Feat-5: morph 增量 SSBO 条目（std430，32 字节），须与 forward_shaded_morph.vert
// 的 MorphDelta { vec4 dpos; vec4 dnrm; } 逐字段一致。布局 [target*vertex_count+vertex]（世界空间增量）。
struct GpuMorphDelta {
    float dx, dy, dz, dpad;   // dpos.xyz（+pad）
    float nx, ny, nz, npad;   // dnrm.xyz（+pad）
};
static_assert(sizeof(GpuMorphDelta) == 32, "GpuMorphDelta std430 = 32 bytes");

// Final-Feat-5: 处理 morph target 数量上限（仅 CPU 侧合并循环的安全护栏，与着色器无关）。
constexpr int kMaxFwdMorphTargets = 64;

// Final-Feat-4: 从 ShadedSpotLight 列表填充 SpotLightsUBO（复用 ubo_types.h SpotLightEntry 布局，
// ≤64 超出截断）。count=0 时着色器 SpotLightsLo 循环不执行 → 无聚光灯贡献（不回归）。
void FillSpotLightsUBO(const std::vector<ShadedSpotLight>& spot_lights, SpotLightsUBO& out) {
    const int n = static_cast<int>(
        (std::min)(spot_lights.size(), static_cast<size_t>(kMaxSpotLightsUBO)));
    out.u_spot_light_count = n;
    for (int i = 0; i < n; ++i) {
        const ShadedSpotLight& s = spot_lights[i];
        out.u_spot_lights[i].color = s.color;
        out.u_spot_lights[i].intensity = s.intensity;
        out.u_spot_lights[i].position = s.position;
        out.u_spot_lights[i].radius = s.radius;
        out.u_spot_lights[i].direction = s.direction;
        out.u_spot_lights[i].inner_cone = s.inner_cone;
        out.u_spot_lights[i].outer_cone = s.outer_cone;
        out.u_spot_lights[i].cast_shadow = s.cast_shadow ? 1 : 0;
        out.u_spot_lights[i].shadow_index = s.shadow_index;
    }
}

// ============================================================
// 编辑器场景视图模式覆盖（阶段4-M2）。MeshRenderer 高级 shading forward 路径读
// device.GetGlobalRenderState() 的 force_unlit/overdraw_mode 标志并在 PerScene/PerMaterial
// UBO 上生效（wireframe 走 PSO，见 SelectShadedPso），语义与执行器 DrawMeshBatch 路径一致：
//  - force_unlit：PerScene light_dir_and_enabled.w 置 0 → forward_shaded.frag 走 Unlit 分支（纯 albedo）。
//  - overdraw   ：PerMaterial 覆盖为固定低强度材质，配合加性混合 PSO 以亮度叠加显示过度绘制。
// ============================================================

/// force_unlit：关方向光（等价 light.enabled=0）。其余光照/材质不变。
void ApplyEditorSceneOverride(RhiDevice& device, FwdPerSceneUBO& scene) {
    if (device.GetGlobalRenderState().force_unlit) {
        scene.light_dir_and_enabled.w = 0.0f;
    }
}

/// overdraw：固定低强度材质（与 draw_executor_common.h PreparePerMaterialUBO 的 overdraw 分支一致）。
void ApplyEditorMaterialOverride(RhiDevice& device, FwdShadedMaterialUBO& mat) {
    if (device.GetGlobalRenderState().overdraw_mode) {
        mat.albedo = glm::vec4(0.1f, 0.04f, 0.02f, 0.0f);
        mat.roughness_ao = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
        mat.emissive = glm::vec4(0.0f);
        mat.flags = glm::vec4(0.0f);
    }
}

} // namespace

unsigned int MeshRenderer::SelectShadedPso(RhiDevice& device, const ShadedMaterial& material) {
    const auto& grs = device.GetGlobalRenderState();
    if (grs.wireframe_mode) return pso_wireframe_;
    if (grs.overdraw_mode) return pso_overdraw_;
    unsigned int pso = material.double_sided ? pso_no_cull_ : pso_;
    if (material.wboit_mode == 1) pso = pso_wboit_accum_;
    else if (material.wboit_mode == 2) pso = pso_wboit_reveal_;
    return pso;
}

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
    // Final-Feat-8: 1x1 白色 cube（6 面），点光 shadow cube 缺省槽回退。采样得 .r=1.0 →
    // closestDepth=radius，(cur-bias)>radius 恒假 → 不产生阴影。保证三后端 cube descriptor 维度匹配。
    const unsigned char* white_faces[6] = {white, white, white, white, white, white};
    white_cube_tex_ = device.CreateTextureCube(1, 1, white_faces, /*linear_filter=*/true);

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

void MeshRenderer::EnsureUnlit2DResources(RhiDevice& device) {
    // 无光照 2D 的三个混合 PSO（与 SpriteBatchRenderer::PsoForBlend 一致）：关深度测试/写入/剔除。
    // alpha 默认：color = SrcAlpha/OneMinusSrcAlpha，alpha 通道 One/OneMinusSrcAlpha（分离）。
    if (pso_unlit2d_alpha_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::SrcAlpha;
        desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_unlit2d_alpha_ = device.CreatePipelineState(desc);
    }
    if (pso_unlit2d_additive_ == 0) {  // additive：SrcAlpha/One
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::SrcAlpha;
        desc.blend_dst = BlendFactor::One;
        desc.alpha_blend_src = BlendFactor::SrcAlpha;
        desc.alpha_blend_dst = BlendFactor::One;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_unlit2d_additive_ = device.CreatePipelineState(desc);
    }
    if (pso_unlit2d_multiply_ == 0) {  // multiply：DstColor/Zero
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::DstColor;
        desc.blend_dst = BlendFactor::Zero;
        desc.alpha_blend_src = BlendFactor::DstColor;
        desc.alpha_blend_dst = BlendFactor::Zero;
        desc.depth_test_enabled = false;
        desc.depth_write_enabled = false;
        desc.culling_enabled = false;
        pso_unlit2d_multiply_ = device.CreatePipelineState(desc);
    }
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

void MeshRenderer::EnsureInstanceCapacity(RhiDevice& device, size_t instance_bytes) {
    if (instance_ssbo_ && instance_ssbo_capacity_ >= instance_bytes) return;
    if (instance_ssbo_) device.DeleteGpuBuffer(instance_ssbo_);
    GpuBufferDesc i_desc;
    i_desc.size = instance_bytes;
    i_desc.usage = GpuBufferUsage::kStorage;
    i_desc.is_dynamic = true;
    instance_ssbo_ = device.CreateGpuBuffer(i_desc, nullptr);
    instance_ssbo_capacity_ = instance_bytes;
}

void MeshRenderer::EnsureMorphCapacity(RhiDevice& device, size_t morph_bytes) {
    if (morph_ssbo_ && morph_ssbo_capacity_ >= morph_bytes) return;
    if (morph_ssbo_) device.DeleteGpuBuffer(morph_ssbo_);
    GpuBufferDesc m_desc;
    m_desc.size = morph_bytes;
    m_desc.usage = GpuBufferUsage::kStorage;
    m_desc.is_dynamic = true;
    morph_ssbo_ = device.CreateGpuBuffer(m_desc, nullptr);
    morph_ssbo_capacity_ = morph_bytes;
}

void MeshRenderer::EnsureIndirectBuffer(RhiDevice& device) {
    if (indirect_buffer_) return;
    GpuBufferDesc d_desc;
    d_desc.size = sizeof(DrawElementsIndirectCommand);  // 单条间接绘制命令
    d_desc.usage = GpuBufferUsage::kIndirect;
    d_desc.is_dynamic = true;
    indirect_buffer_ = device.CreateGpuBuffer(d_desc, nullptr);
}

void MeshRenderer::EnsureShadedResources(RhiDevice& device) {
    // 不剔除 PSO（double-sided 用），与 pso_ 同状态但关背面剔除。
    if (pso_no_cull_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = true;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_no_cull_ = device.CreatePipelineState(desc);
    }
    // WBOIT accumulation PSO（B2c-4）：加性混合（color/alpha 均 ONE/ONE），深度测试开但不写、不剔除，
    // 使各透明片元贡献顺序无关地累加（着色器 wboit_mode=1 输出预乘加权 color/alpha）。
    if (pso_wboit_accum_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::One;
        desc.blend_dst = BlendFactor::One;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::One;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_wboit_accum_ = device.CreatePipelineState(desc);
    }
    // WBOIT revealage PSO（B2c-4）：ZERO/ONE_MINUS_SRC_ALPHA 乘性混合（dst *= (1-srcAlpha)），
    // 深度测试开但不写、不剔除（着色器 wboit_mode=2 输出 (0,0,0,alpha)）。
    if (pso_wboit_reveal_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::Zero;
        desc.blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.alpha_blend_src = BlendFactor::Zero;
        desc.alpha_blend_dst = BlendFactor::OneMinusSrcAlpha;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = false;
        pso_wboit_reveal_ = device.CreatePipelineState(desc);
    }
    // 编辑器线框视图模式 PSO（阶段4-M2）：与 pso_ 同状态（写/测深度、背面剔除、不混合），仅 wireframe=true。
    if (pso_wireframe_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = false;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = true;
        desc.depth_func = CompareFunc::Less;
        desc.culling_enabled = true;
        desc.cull_face = CullFace::Back;
        desc.wireframe = true;
        pso_wireframe_ = device.CreatePipelineState(desc);
    }
    // 编辑器 overdraw 视图模式 PSO（阶段4-M2）：加性混合 ONE/ONE + 深度测试开但不写、不剔除，
    // 配合 ApplyEditorMaterialOverride 的固定低强度材质，使重叠片元以亮度叠加显示过度绘制
    //（与执行器 DX11 SetOverdrawMode / Vulkan overdraw_mode_ 语义一致）。
    if (pso_overdraw_ == 0) {
        PipelineStateDesc desc;
        desc.blend_enabled = true;
        desc.blend_src = BlendFactor::One;
        desc.blend_dst = BlendFactor::One;
        desc.alpha_blend_src = BlendFactor::One;
        desc.alpha_blend_dst = BlendFactor::One;
        desc.depth_test_enabled = true;
        desc.depth_write_enabled = false;
        desc.depth_func = CompareFunc::LessEqual;
        desc.culling_enabled = false;
        pso_overdraw_ = device.CreatePipelineState(desc);
    }
    // 扩展 PerMaterial UBO（160B）。
    if (!per_material_shaded_ubo_) {
        GpuBufferDesc m_desc;
        m_desc.size = sizeof(FwdShadedMaterialUBO);
        m_desc.usage = GpuBufferUsage::kUniform;
        m_desc.is_dynamic = true;
        per_material_shaded_ubo_ = device.CreateGpuBuffer(m_desc, nullptr);
    }
    // 点光 UBO（3088B，binding=3；count=0 时退化为纯方向光，输出与 B2c-1 一致）。
    if (!per_point_lights_ubo_) {
        GpuBufferDesc p_desc;
        p_desc.size = sizeof(PointLightsUBO);
        p_desc.usage = GpuBufferUsage::kUniform;
        p_desc.is_dynamic = true;
        per_point_lights_ubo_ = device.CreateGpuBuffer(p_desc, nullptr);
    }
    // 地形参数 UBO（48B，slot=4；splat_enabled=0 且 snow_coverage=0 时与 B2c-2 输出一致）。
    if (!per_terrain_ubo_) {
        GpuBufferDesc t_desc;
        t_desc.size = sizeof(TerrainParamsUBO);
        t_desc.usage = GpuBufferUsage::kUniform;
        t_desc.is_dynamic = true;
        per_terrain_ubo_ = device.CreateGpuBuffer(t_desc, nullptr);
    }
    // LightProbe SH UBO（160B，slot=5；sh_enabled=0 时不影响间接光，B2c-5）。
    if (!per_light_probe_ubo_) {
        GpuBufferDesc lp_desc;
        lp_desc.size = sizeof(LightProbeDataUBO);
        lp_desc.usage = GpuBufferUsage::kUniform;
        lp_desc.is_dynamic = true;
        per_light_probe_ubo_ = device.CreateGpuBuffer(lp_desc, nullptr);
    }
    // DDGI 参数 UBO（64B，slot=6；ddgi_enabled=0 时不影响间接光，B2c-5）。
    if (!per_ddgi_ubo_) {
        GpuBufferDesc d_desc;
        d_desc.size = sizeof(FwdDDGIParamsUBO);
        d_desc.usage = GpuBufferUsage::kUniform;
        d_desc.is_dynamic = true;
        per_ddgi_ubo_ = device.CreateGpuBuffer(d_desc, nullptr);
    }
    // 聚光灯 UBO（4112B，set7.b1/slot=7；count=0 时无聚光灯贡献，输出与 Final-Feat-3 一致）。
    if (!per_spot_lights_ubo_) {
        GpuBufferDesc sl_desc;
        sl_desc.size = sizeof(SpotLightsUBO);
        sl_desc.usage = GpuBufferUsage::kUniform;
        sl_desc.is_dynamic = true;
        per_spot_lights_ubo_ = device.CreateGpuBuffer(sl_desc, nullptr);
    }
}

void MeshRenderer::DrawShaded(CommandBuffer& cmd, RhiDevice& device,
                              const std::vector<MeshVertex>& vertices,
                              const std::vector<uint16_t>& indices,
                              const glm::mat4& model,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              const glm::vec3& camera_pos,
                              const ShadedMaterial& material,
                              const DirectionalLight& light,
                              const std::vector<ShadedPointLight>& point_lights,
                              const ShadedGI& gi,
                              const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardShaded);
    if (program == 0) return;  // 该后端未提供高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间（与 Draw 一致）---
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
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    // CSM 方向光阴影：从 device 全局渲染状态取级联矩阵/分裂/atlas 区域（与执行器 DrawMeshBatch 同源）。
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    // clearcoat.z 复用为 wboit_mode（B2c-4）：着色器据此切换 accumulation/revealage 输出。
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    // 点光 UBO（clustered ≤64，超出截断）。
    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    // 地形参数 UBO（splat 4 层 + 积雪；均关闭时输出与 B2c-2 一致）。
    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    // LightProbe SH UBO（slot=5）。sh_enabled=0 时 probe_params.x=0 → 着色器不取 SH。
    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    // DDGI 参数 UBO（slot=6）。ddgi_enabled=0 或无 atlas 时 origin.w=0 → 着色器不取 DDGI。
    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    // PSO 选择：WBOIT 透明通道优先（accumulation/revealage），否则按 double-sided 选剔除状态。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0（B2c-2）
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0（B2c-3）
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0（B2c-5）
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0（B2c-5）
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1（Final-Feat-4）
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    // 地形 splat 纹理（slot 5-9，flat unit 5-9）。未用时绑白纹理保证三后端 descriptor 有定义。
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    // DDGI irradiance atlas（slot 10，flat unit 10）。未启用时绑白纹理保证三后端 descriptor 有定义。
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    // CSM shadow atlas（slot 11，flat unit 11）。无 shadow map（grs.shadow_map[0]==0）或 receive_shadow 关时
    // 绑白纹理：着色器内 receive_shadow 门控已返回 0，白纹理深度=1 也不会产生阴影。
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

BufferHandle MeshRenderer::BuildShadedWorldVertexBuffer(RhiDevice& device,
                                                        const std::vector<MeshVertex>& vertices,
                                                        const glm::mat4& model) {
    if (vertices.empty()) return BufferHandle{};
    // CPU 侧预变换到世界空间（与 DrawShaded 完全同源：位置 model、法线 normal-matrix、切线 model 线性部分）。
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
    GpuBufferDesc vb_desc;
    vb_desc.size = gpu_verts.size() * sizeof(GpuMeshVertex);
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = false;  // 常驻静态缓冲（tiled terrain：上传一次，多 tile 子段复用）
    return device.CreateGpuBuffer(vb_desc, gpu_verts.data());
}

void MeshRenderer::DrawShadedExternal(CommandBuffer& cmd, RhiDevice& device,
                                      const ExternalShadedMesh& mesh,
                                      uint32_t index_count,
                                      uint32_t first_index,
                                      const glm::mat4& view,
                                      const glm::mat4& proj,
                                      const glm::vec3& camera_pos,
                                      const ShadedMaterial& material,
                                      const DirectionalLight& light,
                                      const std::vector<ShadedPointLight>& point_lights,
                                      const ShadedGI& gi,
                                      const std::vector<ShadedSpotLight>& spot_lights) {
    if (index_count == 0 || !mesh.vertex_buffer || !mesh.index_buffer) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardShaded);
    if (program == 0) return;  // 该后端未提供高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // 外部常驻 VB/IB：顶点已是世界空间（BuildShadedWorldVertexBuffer 预变换），故不在此重传/预变换。
    // --- UBO 填充（与 DrawShaded 同源）---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    // 外部常驻缓冲：绑 caller 持有的 VB/IB，按 index_count_override 子段绘制。
    cmd.BindVertexBuffer(mesh.vertex_buffer.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(mesh.index_buffer.raw(), mesh.index_type);
    cmd.DrawIndexed(index_count, first_index, 0);
}

void MeshRenderer::DrawGBuffer(CommandBuffer& cmd, RhiDevice& device,
                               const std::vector<MeshVertex>& vertices,
                               const std::vector<uint16_t>& indices,
                               const glm::mat4& model,
                               const glm::mat4& view,
                               const glm::mat4& proj,
                               unsigned int albedo_tex) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::GBufferMesh);
    if (program == 0) return;  // 该后端未提供 GBuffer-mesh 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间（与 DrawShaded 同源；gPosition 直接取此世界坐标）---
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

    // PerFrame（set0.b0）：vp = proj*view，gbuffer.frag 不读但须保持 descriptor layout 兼容。
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    // PerScene（set1.b0）：占位，仅为保持与 PBR 管线 descriptor layout 一致。
    FwdPerSceneUBO scene{};
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    // 不透明几何 PSO（写/测深度、背面剔除、不混合）：MRT 各 attachment 共用此关混合状态。
    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());  // PerScene @ set1.b0（占位）
    cmd.BindTexture(0u, albedo_tex ? albedo_tex : white_tex_, TextureDim::Tex2D);  // u_texture @ set2.b1
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::DrawBatch(CommandBuffer& cmd, RhiDevice& device,
                             const std::vector<MeshDrawItem>& items,
                             const glm::mat4& view,
                             const glm::mat4& proj) {
    if (items.empty()) return;

    const DrawExecutorGlobalState& grs = device.GetGlobalRenderState();
    const bool depth_only = grs.current_pass_depth_only;
    const bool gbuffer_mode = grs.gbuffer_rendering_mode;
    const glm::vec3 camera_pos = glm::vec3(glm::inverse(view)[3]);

    // --- Shadow-cull 预算（仅 depth-only + ortho 阴影 pass；常数/算法与三后端执行器逐位一致）---
    // is_ortho 用 proj[2][3]≈0 判定（与执行器同式：perspective=-1/ortho=0；clip 修正不改该元素）。
    const bool is_ortho = std::abs(proj[2][3]) < 0.01f;
    const bool shadow_cull_active = depth_only && is_ortho;
    // PreZ（透视 depth-only）：蒙皮实例 VS 骨骼开销极大、阴影收益低，整体跳过（与执行器一致）。
    const bool prez_skip_skinned = depth_only && !is_ortho;
    float shadow_cull_limit = 0.0f;
    size_t shadow_inst_budget = SIZE_MAX;
    if (shadow_cull_active && std::abs(proj[0][0]) > 1e-6f) {
        constexpr float kShadowCullMargin       = 150.0f;
        constexpr float kBudgetOrthoThreshold   = 2000.0f;
        constexpr float kBudgetBaseInstances    = 800.0f;
        constexpr float kBudgetMinInstances     = 64.0f;
        constexpr float kSkinnedShadowSkipOrtho = 1500.0f;
        constexpr float kSkinnedBudgetOrtho     = 400.0f;
        constexpr float kSkinnedBudgetBase      = 200.0f;
        const float ortho_size = 1.0f / proj[0][0];
        shadow_cull_limit = ortho_size + kShadowCullMargin;
        if (ortho_size > kBudgetOrthoThreshold) {
            shadow_inst_budget = static_cast<size_t>(
                std::max(kBudgetBaseInstances * kBudgetOrthoThreshold / ortho_size, kBudgetMinInstances));
        }
        if (ortho_size > kSkinnedShadowSkipOrtho) {
            shadow_inst_budget = 0;
        } else if (ortho_size > kSkinnedBudgetOrtho) {
            shadow_inst_budget = static_cast<size_t>(
                std::max(kSkinnedBudgetBase * kSkinnedBudgetOrtho / ortho_size, 0.0f));
        }
    }

    for (const auto& item : items) {
        // 顶点/索引数据源：优先 shared_vertex_ptr（共享模板），否则 item 内联缓冲（与执行器同序）。
        const BatchVertex* vtx_data = item.shared_vertex_ptr ? item.shared_vertex_ptr : item.vertices.data();
        const uint32_t* idx_data = item.shared_index_ptr ? item.shared_index_ptr : item.indices.data();
        const size_t vtx_count = item.shared_vertex_ptr ? item.shared_vertex_count : item.vertices.size();
        const size_t idx_count = item.shared_index_ptr ? item.shared_index_count : item.indices.size();
        if (vtx_count == 0 || idx_count == 0) continue;

        const bool is_instanced = item.instance_transforms.size() > 1;
        const bool skinned_instanced = item.skinned
            && (!item.per_instance_bones.empty() || !item.bone_palette.empty())
            && is_instanced;
        const bool single_skinned = item.skinned && !is_instanced && !item.bone_matrices.empty();

        // 索引 uint32 → uint16（MeshRenderer 逐变体方法契约为 16 位；cpu_mesh 顶点数 < 65536）。
        std::vector<uint16_t> indices16(idx_count);
        for (size_t k = 0; k < idx_count; ++k)
            indices16[k] = static_cast<uint16_t>(idx_data[k]);

        const ShadedMaterial material = BatchToShadedMaterial(item);
        const DirectionalLight light = BatchToDirLight(item);
        const std::vector<ShadedPointLight> point_lights = BatchToPointLights(item);
        const std::vector<ShadedSpotLight> spot_lights = BatchToSpotLights(item);
        const ShadedGI gi{};  // 与已迁移的 terrain/tree/grass 一致：CPU mesh forward 路径不带 DDGI/SH。

        // --- 实例可见集（仅 instanced）：depth-only ortho 阴影 pass 按预算 + lightspace 裁剪 ---
        std::vector<glm::mat4> vis_models;
        std::vector<int> vis_palette_idx;
        if (is_instanced) {
            if (prez_skip_skinned && skinned_instanced) continue;  // PreZ 跳过蒙皮实例
            const size_t n = item.instance_transforms.size();
            vis_models.reserve(n);
            if (skinned_instanced) vis_palette_idx.reserve(n);
            for (size_t j = 0; j < n; ++j) {
                if (shadow_cull_active) {
                    if (vis_models.size() >= shadow_inst_budget) break;
                    if (shadow_cull_limit > 0.0f) {
                        const glm::vec3 wp(item.instance_transforms[j][3]);
                        const glm::vec4 ls = view * glm::vec4(wp, 1.0f);
                        if (std::abs(ls.x) > shadow_cull_limit || std::abs(ls.y) > shadow_cull_limit)
                            continue;
                    }
                }
                vis_models.push_back(item.instance_transforms[j]);
                if (skinned_instanced) {
                    const int pidx = (j < item.instance_bone_palette_idx.size())
                        ? item.instance_bone_palette_idx[j] : 0;
                    vis_palette_idx.push_back(pidx);
                }
            }
            if (vis_models.empty()) continue;  // 全部被剔除
        }

        // ===== GBuffer 模式（RSM；非 depth-only）：蒙皮/实例在 CPU 展开为静态世界几何 =====
        if (gbuffer_mode) {
            const unsigned int albedo_tex = item.texture_handle;
            if (skinned_instanced) {
                for (size_t j = 0; j < vis_models.size(); ++j) {
                    const int pidx = vis_palette_idx[j];
                    const std::vector<glm::mat4>& pal =
                        (pidx >= 0 && pidx < static_cast<int>(item.bone_palette.size()))
                            ? item.bone_palette[pidx] : item.bone_palette[0];
                    std::vector<MeshVertex> sk = SkinBatchToLocal(vtx_data, vtx_count, pal);
                    DrawGBuffer(cmd, device, sk, indices16, vis_models[j], view, proj, albedo_tex);
                }
            } else if (is_instanced) {
                std::vector<MeshVertex> mverts(vtx_count);
                for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
                for (const auto& mdl : vis_models)
                    DrawGBuffer(cmd, device, mverts, indices16, mdl, view, proj, albedo_tex);
            } else if (single_skinned) {
                std::vector<MeshVertex> sk = SkinBatchToLocal(vtx_data, vtx_count, item.bone_matrices);
                DrawGBuffer(cmd, device, sk, indices16, item.model, view, proj, albedo_tex);
            } else {
                std::vector<MeshVertex> mverts(vtx_count);
                for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
                DrawGBuffer(cmd, device, mverts, indices16, item.model, view, proj, albedo_tex);
            }
            continue;
        }

        // ===== forward / depth-only：复用 forward program（depth-only RT 无颜色附件 → frag 丢弃）=====
        if (skinned_instanced) {
            std::vector<SkinnedMeshVertex> sverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) sverts[i] = BatchToSkinnedVertex(vtx_data[i]);
            DrawSkinnedInstancedShaded(cmd, device, sverts, indices16, vis_models,
                                       item.bone_palette, vis_palette_idx, view, proj, camera_pos,
                                       material, light, point_lights, gi, spot_lights);
        } else if (is_instanced) {
            std::vector<MeshVertex> mverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
            DrawInstancedShaded(cmd, device, mverts, indices16, vis_models, view, proj, camera_pos,
                                material, light, point_lights, gi, spot_lights);
        } else if (single_skinned) {
            std::vector<SkinnedMeshVertex> sverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) sverts[i] = BatchToSkinnedVertex(vtx_data[i]);
            DrawSkinnedShaded(cmd, device, sverts, indices16, item.model, item.bone_matrices,
                              view, proj, camera_pos, material, light, point_lights, gi, spot_lights);
        } else {
            std::vector<MeshVertex> mverts(vtx_count);
            for (size_t i = 0; i < vtx_count; ++i) mverts[i] = BatchToMeshVertex(vtx_data[i]);
            DrawShaded(cmd, device, mverts, indices16, item.model, view, proj, camera_pos,
                       material, light, point_lights, gi, spot_lights);
        }
    }
}

BufferHandle MeshRenderer::BuildShadedLocalVertexBuffer(RhiDevice& device,
                                                        const std::vector<MeshVertex>& vertices) {
    if (vertices.empty()) return BufferHandle{};
    // 局部空间打包（不做 model 预变换；每实例 model 由 VS 按 gl_InstanceIndex 变换）。
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        GpuMeshVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
    }
    GpuBufferDesc vb_desc;
    vb_desc.size = gpu_verts.size() * sizeof(GpuMeshVertex);
    vb_desc.usage = GpuBufferUsage::kVertex;
    vb_desc.is_dynamic = false;  // 常驻静态模板缓冲（多实例/多帧共享）
    return device.CreateGpuBuffer(vb_desc, gpu_verts.data());
}

void MeshRenderer::DrawSharedTemplateInstanced(CommandBuffer& cmd, RhiDevice& device,
                                               const ExternalShadedMesh& tmpl,
                                               uint32_t index_count,
                                               uint32_t first_index,
                                               const std::vector<glm::mat4>& instance_models,
                                               const glm::mat4& view,
                                               const glm::mat4& proj,
                                               const glm::vec3& camera_pos,
                                               const ShadedMaterial& material,
                                               const DirectionalLight& light,
                                               const std::vector<ShadedPointLight>& point_lights,
                                               const ShadedGI& gi,
                                               const std::vector<ShadedSpotLight>& spot_lights) {
    if (index_count == 0 || instance_models.empty() ||
        !tmpl.vertex_buffer || !tmpl.index_buffer) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedShaded);
    if (program == 0) return;  // 该后端未提供实例化高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // 每实例 model 矩阵写入内部实例 SSBO（世界空间，0 基索引）。共享的是「顶点模板」，实例矩阵仍逐次提交。
    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    // 共享模板 VB/IB 由 BuildShadedLocalVertexBuffer + caller 提供，常驻不重传。
    // --- UBO 填充（与 DrawInstancedShaded 同源）---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    // 植被风弯曲（B2b-6，tree）：material.foliage 时喂入 grs 风参，否则保持零 → VS 整段跳过。
    if (material.foliage) {
        const auto& grs_f = device.GetGlobalRenderState();
        frame.foliage_wind = grs_f.foliage_wind;
        frame.foliage_push = grs_f.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1
    // 每实例 model SSBO\@slot 0（与 DrawInstancedShaded 同源）。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    // 共享局部空间模板 VB/IB（caller 持有、常驻），按 index_count_override 子段对每实例绘制。
    cmd.BindVertexBuffer(tmpl.vertex_buffer.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(tmpl.index_buffer.raw(), tmpl.index_type);
    // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达。
    cmd.DrawIndexedInstanced(index_count, static_cast<uint32_t>(instance_models.size()),
                             first_index, 0, 0u);
}

void MeshRenderer::DrawSkinnedShaded(CommandBuffer& cmd, RhiDevice& device,
                                     const std::vector<SkinnedMeshVertex>& vertices,
                                     const std::vector<uint16_t>& indices,
                                     const glm::mat4& model,
                                     const std::vector<glm::mat4>& bone_matrices,
                                     const glm::mat4& view,
                                     const glm::mat4& proj,
                                     const glm::vec3& camera_pos,
                                     const ShadedMaterial& material,
                                     const DirectionalLight& light,
                                     const std::vector<ShadedPointLight>& point_lights,
                                     const ShadedGI& gi,
                                     const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty() || bone_matrices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardSkinnedShaded);
    if (program == 0) return;  // 该后端未提供蒙皮高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

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

    // --- UBO 填充（与 DrawShaded 同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

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

    // PSO 选择：与 DrawShaded 一致（WBOIT 透明优先，否则按 double-sided 选剔除）。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1（Final-Feat-4）
    // 骨骼矩阵 SSBO\@slot 0（三后端通用：GL binding0 / Vulkan 位置0(set7) / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::DrawInstancedShaded(CommandBuffer& cmd, RhiDevice& device,
                                       const std::vector<MeshVertex>& vertices,
                                       const std::vector<uint16_t>& indices,
                                       const std::vector<glm::mat4>& instance_models,
                                       const glm::mat4& view,
                                       const glm::mat4& proj,
                                       const glm::vec3& camera_pos,
                                       const ShadedMaterial& material,
                                       const DirectionalLight& light,
                                       const std::vector<ShadedPointLight>& point_lights,
                                       const ShadedGI& gi,
                                       const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedShaded);
    if (program == 0) return;  // 该后端未提供实例化高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- 顶点打包（局部空间，VS 按实例 model 变换，不在 CPU 预变换） ---
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        GpuMeshVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
    }

    // --- 每实例 model 矩阵写入 SSBO（世界空间，0 基索引） ---
    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO 填充（与 DrawShaded 同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    // 植被风弯曲（B2b-6，grass）：material.foliage 时喂入 grs 风参，否则保持零 → VS 整段跳过。
    if (material.foliage) {
        const auto& grs_f = device.GetGlobalRenderState();
        frame.foliage_wind = grs_f.foliage_wind;
        frame.foliage_push = grs_f.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    // PSO 选择：与 DrawShaded 一致（WBOIT 透明优先，否则按 double-sided 选剔除）。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1（Final-Feat-4）
    // 每实例 model SSBO\@slot 0（三后端通用：GL binding0 / Vulkan 位置0(set7) / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达。
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawSkinnedInstancedShaded(CommandBuffer& cmd, RhiDevice& device,
                                              const std::vector<SkinnedMeshVertex>& vertices,
                                              const std::vector<uint16_t>& indices,
                                              const std::vector<glm::mat4>& instance_models,
                                              const std::vector<std::vector<glm::mat4>>& bone_palettes,
                                              const std::vector<int>& instance_palette_idx,
                                              const glm::mat4& view,
                                              const glm::mat4& proj,
                                              const glm::vec3& camera_pos,
                                              const ShadedMaterial& material,
                                              const DirectionalLight& light,
                                              const std::vector<ShadedPointLight>& point_lights,
                                              const ShadedGI& gi,
                                              const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty() || instance_models.empty() ||
        bone_palettes.empty() || instance_palette_idx.size() != instance_models.size()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardSkinnedInstancedShaded);
    if (program == 0) return;  // 该后端未提供蒙皮×实例化高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- 顶点打包（局部/绑定空间，VS 施骨骼混合 + 每实例 model + vp，不在 CPU 预变换） ---
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

    // --- 骨骼调色板去重密排（绑定→局部空间，不预乘 model），记每份调色板起始下标 ---
    std::vector<int> palette_base(bone_palettes.size());
    std::vector<glm::mat4> packed_bones;
    for (size_t p = 0; p < bone_palettes.size(); ++p) {
        palette_base[p] = static_cast<int>(packed_bones.size());
        packed_bones.insert(packed_bones.end(), bone_palettes[p].begin(), bone_palettes[p].end());
    }
    if (packed_bones.empty()) return;
    const size_t bone_bytes = packed_bones.size() * sizeof(glm::mat4);
    EnsureBoneCapacity(device, bone_bytes);
    if (!bone_ssbo_) return;
    device.UpdateGpuBuffer(bone_ssbo_, 0, bone_bytes, packed_bones.data());

    // --- 每实例 {model, bone_offset}（80B MeshSkinnedInst）写入实例 SSBO（0 基索引） ---
    std::vector<MeshSkinnedInstGPU> insts(instance_models.size());
    for (size_t j = 0; j < instance_models.size(); ++j) {
        const int pi = instance_palette_idx[j];
        if (pi < 0 || pi >= static_cast<int>(palette_base.size())) return;  // 调色板下标越界，整批放弃
        insts[j].model = instance_models[j];
        insts[j].bone_offset = palette_base[pi];
        insts[j].pad0 = insts[j].pad1 = insts[j].pad2 = 0;
    }
    const size_t inst_bytes = insts.size() * sizeof(MeshSkinnedInstGPU);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, insts.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuSkinnedVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO 填充（与 DrawShaded / DrawInstancedShaded 同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    // 聚光灯 UBO（set7.b1/slot=7，Final-Feat-4）。count=0 时着色器无聚光灯贡献。
    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

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

    // PSO 选择：与 DrawShaded 一致（WBOIT 透明优先，否则按 double-sided 选剔除）。
    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight @ set7.b1（Final-Feat-4）
    // 实例 SSBO\@slot0（set8.b0）+ 骨骼 SSBO\@slot1（set8.b1）；三后端 @SSBO_LOW_REGISTERS → DX11 t0/t1、GL binding0/1、Vulkan rank0/1。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindStorageBuffer(1u, bone_ssbo_.raw(), 0u, static_cast<uint32_t>(bone_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuSkinnedVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达。
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawMorphShaded(CommandBuffer& cmd, RhiDevice& device,
                                   const std::vector<MeshVertex>& vertices,
                                   const std::vector<uint16_t>& indices,
                                   const std::vector<MeshMorphTarget>& morph_targets,
                                   const glm::mat4& model,
                                   const glm::mat4& view,
                                   const glm::mat4& proj,
                                   const glm::vec3& camera_pos,
                                   const ShadedMaterial& material,
                                   const DirectionalLight& light,
                                   const std::vector<ShadedPointLight>& point_lights,
                                   const ShadedGI& gi,
                                   const std::vector<ShadedSpotLight>& spot_lights) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardMorphShaded);
    if (program == 0) return;  // 该后端未提供 morph 高级 shading 内建着色器

    EnsureResources(device);
    EnsureShadedResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_shaded_ubo_ ||
        !per_point_lights_ubo_ || !per_terrain_ubo_) return;

    // --- CPU 侧预变换基顶点到世界空间（与 DrawShaded 一致）---
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

    // --- morph 增量：CPU 侧按权重线性合并为每顶点单条世界空间增量，打包成 SSBO ---
    // morph 为线性混合 final = base + Σ_t w_t·delta_t，故权重在 CPU 叠加（与本路径既有「顶点
    // CPU 预变换」一致，且规避 VS 侧额外 UBO 在三后端绑定语义上的分歧）。仅收纳 deltas 与基网格
    // 等长的有效 target（≤64）；位置增量用 model 线性部分、法线增量用法线矩阵（不含平移）。
    std::vector<GpuMorphDelta> morph_deltas(vertices.size());  // 值初始化为 0
    int active_targets = 0;
    for (const MeshMorphTarget& tgt : morph_targets) {
        if (tgt.position_deltas.size() != vertices.size()) continue;  // 跳过不匹配 target
        if (active_targets >= kMaxFwdMorphTargets) break;
        ++active_targets;
        const float w = tgt.weight;
        if (w == 0.0f) continue;  // 零权重无贡献
        const bool has_nrm = tgt.normal_deltas.size() == vertices.size();
        for (size_t i = 0; i < vertices.size(); ++i) {
            const glm::vec3 wdp = model3 * tgt.position_deltas[i];
            const glm::vec3 wdn = has_nrm ? normal_matrix * tgt.normal_deltas[i] : glm::vec3(0.0f);
            GpuMorphDelta& d = morph_deltas[i];
            d.dx += w * wdp.x; d.dy += w * wdp.y; d.dz += w * wdp.z;
            d.nx += w * wdn.x; d.ny += w * wdn.y; d.nz += w * wdn.z;
        }
    }
    if (morph_deltas.empty()) morph_deltas.push_back(GpuMorphDelta{});  // 保证 SSBO 非空可绑定

    const size_t morph_bytes = morph_deltas.size() * sizeof(GpuMorphDelta);
    EnsureMorphCapacity(device, morph_bytes);
    if (!morph_ssbo_) return;
    device.UpdateGpuBuffer(morph_ssbo_, 0, morph_bytes, morph_deltas.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- UBO 填充（与 DrawShaded 同构） ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(camera_pos, 1.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    FwdPerSceneUBO scene{};
    const glm::vec3 to_light = glm::normalize(-light.direction);
    scene.light_dir_and_enabled = glm::vec4(to_light, light.enabled ? 1.0f : 0.0f);
    scene.light_color_and_ambient = glm::vec4(light.color, light.ambient);
    const auto& grs = device.GetGlobalRenderState();
    scene.light_params = glm::vec4(light.intensity,
                                   material.shadow_strength,
                                   material.receive_shadow ? 1.0f : 0.0f, 0.0f);
    scene.cascade_splits = glm::vec4(grs.cascade_splits[0], grs.cascade_splits[1],
                                     grs.cascade_splits[2], 0.0f);
    for (int i = 0; i < 3; ++i) {
        scene.light_space_matrices[i] = grs.light_space_matrix[i];
        scene.shadow_atlas_regions[i] = grs.shadow_atlas_region[i];
    }
    // Final-Feat-8: 聚光灯 light-space 矩阵（点/聚光阴影接收，与执行器 DrawMeshBatch 同源）。
    for (int i = 0; i < 4; ++i)
        scene.spot_light_space_matrices[i] = grs.spot_light_space_matrix[i];
    ApplyEditorSceneOverride(device, scene);
    device.UpdateGpuBuffer(per_scene_ubo_, 0, sizeof(scene), &scene);

    FwdShadedMaterialUBO mat{};
    mat.albedo = glm::vec4(material.albedo, material.metallic);
    mat.roughness_ao = glm::vec4(material.roughness, material.ao,
                                 material.normal_strength, material.alpha_cutoff);
    mat.emissive = glm::vec4(material.emissive, material.alpha_test ? 1.0f : 0.0f);
    mat.flags = glm::vec4(material.normal_tex ? 1.0f : 0.0f,
                          material.metallic_roughness_tex ? 1.0f : 0.0f,
                          material.emissive_tex ? 1.0f : 0.0f,
                          material.occlusion_tex ? 1.0f : 0.0f);
    mat.mode_params = glm::vec4(static_cast<float>(material.shading_mode),
                                material.double_sided ? 1.0f : 0.0f,
                                material.anisotropy, material.pom_height_scale);
    mat.sss = glm::vec4(material.sss_tint, material.sss_strength);
    mat.clearcoat = glm::vec4(material.clear_coat, material.clear_coat_roughness,
                              static_cast<float>(material.wboit_mode), 0.0f);
    mat.toon_shadow = glm::vec4(material.toon_shadow_color, material.toon_shadow_threshold);
    mat.toon_params = glm::vec4(material.toon_shadow_softness, material.toon_specular_size,
                                material.toon_specular_strength, material.toon_rim_strength);
    mat.watercolor = glm::vec4(material.watercolor_paper_strength, material.watercolor_edge_darkening,
                               material.watercolor_color_bleed, material.watercolor_pigment_density);
    ApplyEditorMaterialOverride(device, mat);
    device.UpdateGpuBuffer(per_material_shaded_ubo_, 0, sizeof(mat), &mat);

    PointLightsUBO plights{};
    const int pl_count = static_cast<int>(
        (std::min)(point_lights.size(), static_cast<size_t>(kMaxPointLightsUBO)));
    plights.u_point_light_count = pl_count;
    for (int i = 0; i < pl_count; ++i) {
        const ShadedPointLight& pl = point_lights[i];
        plights.u_point_lights[i].color = pl.color;
        plights.u_point_lights[i].intensity = pl.intensity;
        plights.u_point_lights[i].position = pl.position;
        plights.u_point_lights[i].radius = pl.radius;
        plights.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        plights.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    device.UpdateGpuBuffer(per_point_lights_ubo_, 0, sizeof(plights), &plights);

    TerrainParamsUBO terrain{};
    terrain.u_splat_enabled = material.splat_enabled ? 1.0f : 0.0f;
    terrain.u_snow_coverage = material.snow_coverage;
    terrain.u_snow_normal_threshold = material.snow_normal_threshold;
    terrain.u_snow_edge_sharpness = material.snow_edge_sharpness;
    terrain.u_splat_tiling = material.splat_tiling;
    terrain.u_snow_params = glm::vec4(material.snow_albedo, material.snow_roughness);
    device.UpdateGpuBuffer(per_terrain_ubo_, 0, sizeof(terrain), &terrain);

    LightProbeDataUBO probe{};
    for (int i = 0; i < 9; ++i) probe.sh_coefficients[i] = gi.sh_coefficients[i];
    probe.probe_params = glm::vec4(gi.sh_enabled ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_light_probe_ubo_, 0, sizeof(probe), &probe);

    const bool ddgi_on = gi.ddgi_enabled && gi.ddgi_irradiance_atlas != 0;
    FwdDDGIParamsUBO ddgi{};
    ddgi.origin = glm::vec4(gi.ddgi_grid_origin, ddgi_on ? 1.0f : 0.0f);
    ddgi.spacing = glm::vec4(gi.ddgi_grid_spacing, gi.ddgi_gi_intensity);
    ddgi.resolution = glm::ivec4(gi.ddgi_grid_resolution, gi.ddgi_irradiance_texels);
    ddgi.misc = glm::vec4(gi.ddgi_normal_bias, 0.0f, 0.0f, 0.0f);
    device.UpdateGpuBuffer(per_ddgi_ubo_, 0, sizeof(ddgi), &ddgi);

    SpotLightsUBO slights{};
    FillSpotLightsUBO(spot_lights, slights);
    device.UpdateGpuBuffer(per_spot_lights_ubo_, 0, sizeof(slights), &slights);

    auto tex_or_white = [&](unsigned int h) { return h ? h : white_tex_; };

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    unsigned int pso = SelectShadedPso(device, material);
    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());            // PerFrame    @ set0.b0
    cmd.BindUniformBuffer(1u, per_scene_ubo_.raw());            // PerScene    @ set1.b0
    cmd.BindUniformBuffer(2u, per_material_shaded_ubo_.raw());  // PerMaterial @ set2.b0（扩展）
    cmd.BindUniformBuffer(3u, per_point_lights_ubo_.raw());     // PointLights  @ set3.b0
    cmd.BindUniformBuffer(4u, per_terrain_ubo_.raw());          // TerrainParams@ set4.b0
    cmd.BindUniformBuffer(5u, per_light_probe_ubo_.raw());      // FwdLightProbe@ set5.b0
    cmd.BindUniformBuffer(6u, per_ddgi_ubo_.raw());             // FwdDDGI      @ set6.b0
    cmd.BindTexture(0u, tex_or_white(material.albedo_tex), TextureDim::Tex2D);
    cmd.BindTexture(1u, tex_or_white(material.normal_tex), TextureDim::Tex2D);
    cmd.BindTexture(2u, tex_or_white(material.metallic_roughness_tex), TextureDim::Tex2D);
    cmd.BindTexture(3u, tex_or_white(material.emissive_tex), TextureDim::Tex2D);
    cmd.BindTexture(4u, tex_or_white(material.occlusion_tex), TextureDim::Tex2D);
    cmd.BindTexture(5u, tex_or_white(material.splat_weight_map), TextureDim::Tex2D);
    cmd.BindTexture(6u, tex_or_white(material.splat_layers[0]), TextureDim::Tex2D);
    cmd.BindTexture(7u, tex_or_white(material.splat_layers[1]), TextureDim::Tex2D);
    cmd.BindTexture(8u, tex_or_white(material.splat_layers[2]), TextureDim::Tex2D);
    cmd.BindTexture(9u, tex_or_white(material.splat_layers[3]), TextureDim::Tex2D);
    cmd.BindTexture(10u, tex_or_white(gi.ddgi_irradiance_atlas), TextureDim::Tex2D);
    cmd.BindTexture(11u, tex_or_white(grs.shadow_map[0]), TextureDim::Tex2D);
    // Final-Feat-8: 聚光灯 2D 阴影图（flat unit 12-15）/ 点光 cube 阴影（flat unit 16-19）。
    // 未用槽位绑默认白纹理/白 cube（采样得深度=1 → 不产生阴影），保证三后端 descriptor 维度匹配。
    cmd.BindTexture(12u, grs.spot_shadow_map[0] ? grs.spot_shadow_map[0] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(13u, grs.spot_shadow_map[1] ? grs.spot_shadow_map[1] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(14u, grs.spot_shadow_map[2] ? grs.spot_shadow_map[2] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(15u, grs.spot_shadow_map[3] ? grs.spot_shadow_map[3] : white_tex_, TextureDim::Tex2D);
    cmd.BindTexture(16u, grs.point_shadow_map[0] ? grs.point_shadow_map[0] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(17u, grs.point_shadow_map[1] ? grs.point_shadow_map[1] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(18u, grs.point_shadow_map[2] ? grs.point_shadow_map[2] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindTexture(19u, grs.point_shadow_map[3] ? grs.point_shadow_map[3] : white_cube_tex_, TextureDim::TexCube);
    cmd.BindUniformBuffer(7u, per_spot_lights_ubo_.raw());      // FwdSpotLight    @ set7.b1（Final-Feat-4）
    // morph 合并增量 SSBO\@slot 0（三后端通用：GL binding0 / Vulkan SSBO 第0(set7.b0) / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, morph_ssbo_.raw(), 0u, static_cast<uint32_t>(morph_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
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
    ApplyEditorSceneOverride(device, scene);
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
    ApplyEditorSceneOverride(device, scene);
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

void MeshRenderer::DrawDepthOnly(CommandBuffer& cmd, RhiDevice& device,
                                 const std::vector<MeshVertex>& vertices,
                                 const std::vector<uint16_t>& indices,
                                 const glm::mat4& model,
                                 const glm::mat4& view,
                                 const glm::mat4& proj) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrDepth);
    if (program == 0) return;  // 该后端未提供 depth-only 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_) return;

    // --- CPU 侧预变换顶点到世界空间（仅 position 影响深度；normal/tangent 复用以保持布局一致）---
    const glm::mat3 model3 = glm::mat3(model);
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        const glm::vec3 wp = glm::vec3(model * glm::vec4(v.position, 1.0f));
        const glm::vec3 wn = model3 * v.normal;
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

    // --- 仅 PerFrame UBO（shadow.frag 空，不需 scene/material/纹理）---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
        VertexAttr{3u, 3u, 36u},   // normal
        VertexAttr{4u, 3u, 48u},   // tangent
    };

    cmd.SetPipelineState(pso_);             // 写/测深度（Less）、背面剔除
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::DrawDepthOnlyInstanced(CommandBuffer& cmd, RhiDevice& device,
                                          const std::vector<MeshVertex>& vertices,
                                          const std::vector<uint16_t>& indices,
                                          const std::vector<glm::mat4>& instance_models,
                                          const glm::mat4& view,
                                          const glm::mat4& proj,
                                          bool foliage) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedDepth);
    if (program == 0) return;  // 该后端未提供实例化 depth-only 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_) return;

    // --- 局部空间顶点打包（VS 按实例 model 变换，不在 CPU 预变换）---
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        GpuMeshVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
    }

    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- 仅 PerFrame UBO（shadow.frag 空，不需 scene/material/纹理）+ 可选植被风 ---
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    if (foliage) {
        const auto& grs = device.GetGlobalRenderState();
        frame.foliage_wind = grs.foliage_wind;
        frame.foliage_push = grs.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u}, VertexAttr{1u, 4u, 12u}, VertexAttr{2u, 2u, 28u},
        VertexAttr{3u, 3u, 36u}, VertexAttr{4u, 3u, 48u},
    };

    cmd.SetPipelineState(pso_);             // 写/测深度（Less）、背面剔除
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());  // PerFrame @ set0.b0
    // 每实例 model SSBO\@slot 0（与 DrawInstancedShaded 同源）。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 契约：first_instance 恒 0，偏移已由 0 基 SSBO 索引表达。
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawDepthOnlySharedTemplateInstanced(CommandBuffer& cmd, RhiDevice& device,
                                                        const ExternalShadedMesh& tmpl,
                                                        uint32_t index_count,
                                                        uint32_t first_index,
                                                        const std::vector<glm::mat4>& instance_models,
                                                        const glm::mat4& view,
                                                        const glm::mat4& proj,
                                                        bool foliage) {
    if (index_count == 0 || instance_models.empty() ||
        !tmpl.vertex_buffer || !tmpl.index_buffer) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardInstancedDepth);
    if (program == 0) return;

    EnsureResources(device);
    if (!per_frame_ubo_) return;

    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    frame.camera_pos = glm::vec4(0.0f);
    if (foliage) {
        const auto& grs = device.GetGlobalRenderState();
        frame.foliage_wind = grs.foliage_wind;
        frame.foliage_push = grs.foliage_push;
    }
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u}, VertexAttr{1u, 4u, 12u}, VertexAttr{2u, 2u, 28u},
        VertexAttr{3u, 3u, 36u}, VertexAttr{4u, 3u, 48u},
    };

    cmd.SetPipelineState(pso_);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    // 共享局部空间模板 VB/IB（caller 持有、常驻），按子段对每实例绘制。
    cmd.BindVertexBuffer(tmpl.vertex_buffer.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(tmpl.index_buffer.raw(), tmpl.index_type);
    cmd.DrawIndexedInstanced(index_count, static_cast<uint32_t>(instance_models.size()),
                             first_index, 0, 0u);
}

void MeshRenderer::DrawInstanced(CommandBuffer& cmd, RhiDevice& device,
                                 const std::vector<MeshVertex>& vertices,
                                 const std::vector<uint16_t>& indices,
                                 const std::vector<glm::mat4>& instance_models,
                                 const glm::mat4& view,
                                 const glm::mat4& proj,
                                 const glm::vec3& camera_pos,
                                 const MeshMaterial& material,
                                 const DirectionalLight& light) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrInstanced);
    if (program == 0) return;  // 该后端未提供实例化 forward PBR 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- 顶点打包（局部空间，VS 按实例 model 变换，不在 CPU 预变换） ---
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        GpuMeshVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
    }

    // --- 每实例 model 矩阵写入 SSBO（世界空间，0 基索引） ---
    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
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
    ApplyEditorSceneOverride(device, scene);
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
    // 每实例 model SSBO\@slot 0（三后端通用语义：GL binding0 / Vulkan 位置0 / DX11 t0 经 @SSBO_LOW_REGISTERS）。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 契约：first_instance 恒 0，DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达。
    cmd.DrawIndexedInstanced(static_cast<uint32_t>(indices.size()),
                             static_cast<uint32_t>(instance_models.size()),
                             0u, 0, 0u);
}

void MeshRenderer::DrawIndirect(CommandBuffer& cmd, RhiDevice& device,
                                const std::vector<MeshVertex>& vertices,
                                const std::vector<uint16_t>& indices,
                                const std::vector<glm::mat4>& instance_models,
                                const glm::mat4& view,
                                const glm::mat4& proj,
                                const glm::vec3& camera_pos,
                                const MeshMaterial& material,
                                const DirectionalLight& light) {
    if (vertices.empty() || indices.empty() || instance_models.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::ForwardPbrInstanced);
    if (program == 0) return;  // 该后端未提供实例化 forward PBR 内建着色器

    EnsureResources(device);
    if (!per_frame_ubo_ || !per_scene_ubo_ || !per_material_ubo_) return;

    // --- 顶点打包（局部空间，VS 按实例 model 变换，与 DrawInstanced 同构） ---
    std::vector<GpuMeshVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const MeshVertex& v = vertices[i];
        GpuMeshVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
        g.nx = v.normal.x; g.ny = v.normal.y; g.nz = v.normal.z;
        g.tx = v.tangent.x; g.ty = v.tangent.y; g.tz = v.tangent.z;
    }

    // --- 每实例 model 矩阵写入 SSBO（世界空间，0 基索引） ---
    const size_t inst_bytes = instance_models.size() * sizeof(glm::mat4);
    EnsureInstanceCapacity(device, inst_bytes);
    if (!instance_ssbo_) return;
    device.UpdateGpuBuffer(instance_ssbo_, 0, inst_bytes, instance_models.data());

    const size_t vbytes = gpu_verts.size() * sizeof(GpuMeshVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // --- 间接绘制命令写入 indirect buffer（CPU 端填，GPU-driven pass 亦可回写） ---
    // base_instance 恒 0：DX11 SV_InstanceID 从 0 起，偏移已由 0 基 SSBO 索引表达（§6）。
    EnsureIndirectBuffer(device);
    if (!indirect_buffer_) return;
    DrawElementsIndirectCommand draw_cmd{};
    draw_cmd.count = static_cast<uint32_t>(indices.size());
    draw_cmd.instance_count = static_cast<uint32_t>(instance_models.size());
    draw_cmd.first_index = 0u;
    draw_cmd.base_vertex = 0;
    draw_cmd.base_instance = 0u;
    device.UpdateGpuBuffer(indirect_buffer_, 0, sizeof(draw_cmd), &draw_cmd);

    // --- UBO 填充（与静态/实例化路径同构） ---
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
    ApplyEditorSceneOverride(device, scene);
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
    // 每实例 model SSBO\@slot 0（与 DrawInstanced 同语义）。
    cmd.BindStorageBuffer(0u, instance_ssbo_.raw(), 0u, static_cast<uint32_t>(inst_bytes));
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuMeshVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    // 间接绘制：绘制参数取自 indirect buffer 偏移 0 处的 DrawElementsIndirectCommand。
    cmd.DrawIndexedIndirect(indirect_buffer_.raw(), 0u);
}

void MeshRenderer::DrawUnlit2D(CommandBuffer& cmd, RhiDevice& device,
                              const std::vector<Unlit2DVertex>& vertices,
                              const std::vector<uint16_t>& indices,
                              const glm::mat4& view,
                              const glm::mat4& proj,
                              unsigned int texture,
                              unsigned int blend_mode) {
    if (vertices.empty() || indices.empty()) return;

    unsigned int program = device.GetBuiltinProgram(BuiltinProgram::Sprite2D);
    if (program == 0) return;  // 该后端未提供 sprite2d 内建着色器

    EnsureResources(device);          // 复用 per_frame_ubo_（176B，vp 在首）/ vbo_ / ibo_ / white_tex_
    EnsureUnlit2DResources(device);   // 懒建无光照 2D 混合 PSO
    if (!per_frame_ubo_) return;

    // 顶点已是世界空间（spine runtime computeWorldVertices 已做 2D 蒙皮）；按 Sprite2D 布局打包。
    std::vector<GpuUnlit2DVertex> gpu_verts(vertices.size());
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Unlit2DVertex& v = vertices[i];
        GpuUnlit2DVertex& g = gpu_verts[i];
        g.px = v.position.x; g.py = v.position.y; g.pz = v.position.z;
        g.r = v.color.r; g.g = v.color.g; g.b = v.color.b; g.a = v.color.a;
        g.u = v.uv.x; g.v = v.uv.y;
    }

    const size_t vbytes = gpu_verts.size() * sizeof(GpuUnlit2DVertex);
    const size_t ibytes = indices.size() * sizeof(uint16_t);
    EnsureVertexCapacity(device, vbytes);
    EnsureIndexCapacity(device, ibytes);
    if (!vbo_ || !ibo_) return;
    device.UpdateGpuBuffer(vbo_, 0, vbytes, gpu_verts.data());
    device.UpdateGpuBuffer(ibo_, 0, ibytes, indices.data());

    // Sprite2D PerFrame 块（FwdPerFrameUBO 与 SpritePerFrameUBO 同布局，着色器仅用 vp）。
    FwdPerFrameUBO frame{};
    frame.vp = proj * view;
    frame.view = view;
    device.UpdateGpuBuffer(per_frame_ubo_, 0, sizeof(frame), &frame);

    const std::vector<VertexAttr> attrs = {
        VertexAttr{0u, 3u, 0u},    // pos
        VertexAttr{1u, 4u, 12u},   // color
        VertexAttr{2u, 2u, 28u},   // uv
    };

    unsigned int pso = pso_unlit2d_alpha_;
    if (blend_mode == 1) pso = pso_unlit2d_additive_;
    else if (blend_mode == 2) pso = pso_unlit2d_multiply_;

    cmd.SetPipelineState(pso);
    cmd.BindShaderProgram(program);
    cmd.BindUniformBuffer(0u, per_frame_ubo_.raw());                       // PerFrame @ set0.b0（仅 vp）
    cmd.BindTexture(0u, texture ? texture : white_tex_, TextureDim::Tex2D); // u_texture @ slot 0
    cmd.BindVertexBuffer(vbo_.raw(), static_cast<uint32_t>(sizeof(GpuUnlit2DVertex)), attrs);
    cmd.BindIndexBuffer(ibo_.raw(), IndexType::UInt16);
    cmd.DrawIndexed(static_cast<uint32_t>(indices.size()), 0u, 0);
}

void MeshRenderer::Shutdown(RhiDevice& device) {
    if (vbo_) device.DeleteGpuBuffer(vbo_);
    if (ibo_) device.DeleteGpuBuffer(ibo_);
    if (per_frame_ubo_) device.DeleteGpuBuffer(per_frame_ubo_);
    if (per_scene_ubo_) device.DeleteGpuBuffer(per_scene_ubo_);
    if (per_material_ubo_) device.DeleteGpuBuffer(per_material_ubo_);
    if (per_material_shaded_ubo_) device.DeleteGpuBuffer(per_material_shaded_ubo_);
    if (per_point_lights_ubo_) device.DeleteGpuBuffer(per_point_lights_ubo_);
    if (per_terrain_ubo_) device.DeleteGpuBuffer(per_terrain_ubo_);
    if (per_light_probe_ubo_) device.DeleteGpuBuffer(per_light_probe_ubo_);
    if (per_ddgi_ubo_) device.DeleteGpuBuffer(per_ddgi_ubo_);
    if (bone_ssbo_) device.DeleteGpuBuffer(bone_ssbo_);
    if (instance_ssbo_) device.DeleteGpuBuffer(instance_ssbo_);
    if (indirect_buffer_) device.DeleteGpuBuffer(indirect_buffer_);
    vbo_ = ibo_ = per_frame_ubo_ = per_scene_ubo_ = per_material_ubo_ = BufferHandle{};
    per_material_shaded_ubo_ = BufferHandle{};
    per_point_lights_ubo_ = BufferHandle{};
    per_terrain_ubo_ = BufferHandle{};
    per_light_probe_ubo_ = BufferHandle{};
    per_ddgi_ubo_ = BufferHandle{};
    bone_ssbo_ = BufferHandle{};
    instance_ssbo_ = BufferHandle{};
    indirect_buffer_ = BufferHandle{};
    vbo_capacity_ = ibo_capacity_ = bone_ssbo_capacity_ = instance_ssbo_capacity_ = 0;
    init_ = false;
}

} // namespace render
} // namespace dse
