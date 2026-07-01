/**
 * @file mesh_renderer_internal.h
 * @brief Shared internal helpers for MeshRenderer split implementation files.
 *
 * Included by mesh_renderer.cpp, mesh_renderer_shaded.cpp,
 * mesh_renderer_shadow.cpp, mesh_renderer_instancing.cpp.
 */
#ifndef DSE_RENDER_MESH_RENDERER_INTERNAL_H
#define DSE_RENDER_MESH_RENDERER_INTERNAL_H

#include "engine/render/mesh_renderer.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/ubo_types.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#include <glm/gtc/matrix_inverse.hpp>

namespace dse {
namespace render {
namespace mesh_internal {

struct GpuMeshVertex {
    float px, py, pz;
    float r, g, b, a;
    float u, v;
    float nx, ny, nz;
    float tx, ty, tz;
};
static_assert(sizeof(GpuMeshVertex) == 60, "GpuMeshVertex must be tightly packed (3+4+2+3+3 floats)");

struct GpuUnlit2DVertex {
    float px, py, pz;
    float r, g, b, a;
    float u, v;
};
static_assert(sizeof(GpuUnlit2DVertex) == 36, "GpuUnlit2DVertex must be tightly packed (3+4+2 floats)");

struct GpuSkinnedVertex {
    float px, py, pz;
    float r, g, b, a;
    float u, v;
    float nx, ny, nz;
    float tx, ty, tz;
    float bi0, bi1, bi2, bi3;
    float bw0, bw1, bw2, bw3;
};
static_assert(sizeof(GpuSkinnedVertex) == 92, "GpuSkinnedVertex must be tightly packed (3+4+2+3+3+4+4 floats)");

struct MeshSkinnedInstGPU {
    glm::mat4 model;
    int32_t bone_offset;
    int32_t pad0, pad1, pad2;
};
static_assert(sizeof(MeshSkinnedInstGPU) == 80, "MeshSkinnedInstGPU std430 = 80 bytes (mat4 + int + 3 pad)");

inline MeshVertex BatchToMeshVertex(const BatchVertex& bv) {
    MeshVertex mv;
    mv.position = bv.pos;
    mv.color = bv.color;
    mv.uv = bv.uv;
    mv.normal = bv.normal;
    mv.tangent = bv.tangent;
    return mv;
}

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

inline DirectionalLight BatchToDirLight(const MeshDrawItem& it) {
    DirectionalLight l;
    l.direction = it.light_direction;
    l.color = it.light_color;
    l.intensity = it.light_intensity;
    l.ambient = it.ambient_intensity;
    l.enabled = it.lighting_enabled;
    return l;
}

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

struct FwdPerFrameUBO {
    glm::mat4 vp;
    glm::mat4 view;
    glm::vec4 camera_pos;
    glm::vec4 foliage_wind;
    glm::vec4 foliage_push;
};
static_assert(sizeof(FwdPerFrameUBO) == 176, "FwdPerFrameUBO std140 = 176 bytes");

struct FwdPerSceneUBO {
    glm::vec4 light_dir_and_enabled;
    glm::vec4 light_color_and_ambient;
    glm::vec4 light_params;
    glm::vec4 cascade_splits;
    glm::mat4 light_space_matrices[3];
    glm::vec4 shadow_atlas_regions[3];
    glm::mat4 spot_light_space_matrices[4];
};
static_assert(sizeof(FwdPerSceneUBO) == 560, "FwdPerSceneUBO std140 = 560 bytes");

struct FwdPerMaterialUBO {
    glm::vec4 albedo;
    glm::vec4 roughness_ao;
    glm::vec4 emissive;
    glm::vec4 flags;
};
static_assert(sizeof(FwdPerMaterialUBO) == 64, "FwdPerMaterialUBO std140 = 64 bytes");

struct FwdShadedMaterialUBO {
    glm::vec4 albedo;
    glm::vec4 roughness_ao;
    glm::vec4 emissive;
    glm::vec4 flags;
    glm::vec4 mode_params;
    glm::vec4 sss;
    glm::vec4 clearcoat;
    glm::vec4 toon_shadow;
    glm::vec4 toon_params;
    glm::vec4 watercolor;
};
static_assert(sizeof(FwdShadedMaterialUBO) == 160, "FwdShadedMaterialUBO std140 = 160 bytes");

struct GpuMorphDelta {
    float dx, dy, dz, dpad;
    float nx, ny, nz, npad;
};
static_assert(sizeof(GpuMorphDelta) == 32, "GpuMorphDelta std430 = 32 bytes");

constexpr int kMaxFwdMorphTargets = 64;

inline void FillSpotLightsUBO(const std::vector<ShadedSpotLight>& spot_lights, SpotLightsUBO& out) {
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

inline void ApplyEditorSceneOverride(RhiDevice& device, FwdPerSceneUBO& scene) {
    if (device.GetGlobalRenderState().force_unlit) {
        scene.light_dir_and_enabled.w = 0.0f;
    }
}

inline void ApplyEditorMaterialOverride(RhiDevice& device, FwdShadedMaterialUBO& mat) {
    if (device.GetGlobalRenderState().overdraw_mode) {
        mat.albedo = glm::vec4(0.1f, 0.04f, 0.02f, 0.0f);
        mat.roughness_ao = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
        mat.emissive = glm::vec4(0.0f);
        mat.flags = glm::vec4(0.0f);
    }
}

} // namespace mesh_internal
} // namespace render
} // namespace dse

#endif // DSE_RENDER_MESH_RENDERER_INTERNAL_H
