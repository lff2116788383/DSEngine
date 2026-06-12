/**
 * @file draw_executor_common.h
 * @brief 三后端 DrawExecutor 共享的全局渲染状态与辅助函数
 *
 * 从 GL/DX11/Vulkan DrawExecutor 中提取的完全相同代码：
 * - 全局阴影/光源矩阵存储与 setter
 * - Light Probe SH 球谐系数
 * - 渲染统计帧管理
 * - PerScene/PerMaterial UBO 数据准备（从 MeshDrawItem 填充共享 UBO 结构体）
 *
 * RhiDevice 持有唯一 DrawExecutorGlobalState 实例，各后端 executor 通过引用访问。
 */

#ifndef DSE_RENDER_DRAW_EXECUTOR_COMMON_H
#define DSE_RENDER_DRAW_EXECUTOR_COMMON_H

#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/ubo_types.h"

namespace dse {
namespace render {

// ============================================================
// 全局渲染状态（三端完全相同的成员变量 + setter）
// ============================================================

struct DrawExecutorGlobalState {
    // --- 方向光 CSM ---
    glm::mat4 light_space_matrix[3] = {};
    float cascade_splits[3] = {};
    unsigned int shadow_map[3] = {};
    glm::vec4 shadow_atlas_region[3] = {
        glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 0.0f, 0.0f),
        glm::vec4(1.0f, 1.0f, 0.0f, 0.0f)
    };

    // --- 聚光灯 ---
    glm::mat4 spot_light_space_matrix[4] = {
        glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)
    };
    unsigned int spot_shadow_map[4] = {};

    // --- 点光源 ---
    unsigned int point_shadow_map[4] = {};

    // --- Light Probe ---
    glm::vec4 light_probe_sh[9] = {};
    bool light_probe_enabled = false;

    // --- DDGI ---
    bool ddgi_enabled = false;
    unsigned int ddgi_irradiance_atlas = 0;
    glm::vec3 ddgi_grid_origin = glm::vec3(0.0f);
    glm::vec3 ddgi_grid_spacing = glm::vec3(1.0f);
    glm::ivec3 ddgi_grid_resolution = glm::ivec3(0);
    int ddgi_irradiance_texels = 8;
    float ddgi_gi_intensity = 1.0f;
    float ddgi_normal_bias = 0.2f;

    // --- GBuffer (Deferred) ---
    static constexpr int kMaxGBufferTextures = 4;
    unsigned int gbuffer_texture[kMaxGBufferTextures] = {};
    bool gbuffer_rendering_mode = false;  ///< true: DrawMeshBatch 使用 GBuffer shader

    // --- 全局湿度（天气系统驱动 PBR 湿表面效果） ---
    float global_wetness = 0.0f;

    // --- 植被风参数 ---
    glm::vec4 foliage_wind = glm::vec4(0.0f);   ///< x=time, y=strength, z=wind_dir_x, w=wind_dir_z
    glm::vec4 foliage_push = glm::vec4(0.0f);   ///< xyz=character_pos, w=push_radius

    // --- 编辑器场景视图模式 ---
    bool force_unlit = false;     ///< Unlit 模式: shader 跳过光照计算
    bool overdraw_mode = false;   ///< Overdraw 模式: 固定颜色叠加输出
    bool wireframe_mode = false;  ///< Wireframe 模式: 线框渲染

    // --- 渲染统计 ---
    RenderStats current_frame_stats;
    RenderStats last_frame_stats;

    // ---- Setter 方法 ----

    void SetShadowMap(unsigned int index, unsigned int handle) {
        if (index < 3) shadow_map[index] = handle;
    }
    void SetSpotShadowMap(unsigned int index, unsigned int handle) {
        if (index < 4) spot_shadow_map[index] = handle;
    }
    void SetPointShadowMap(unsigned int index, unsigned int handle) {
        if (index < 4) point_shadow_map[index] = handle;
    }
    void SetLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 3) light_space_matrix[index] = mat;
    }
    void SetCascadeSplit(unsigned int index, float split) {
        if (index < 3) cascade_splits[index] = split;
    }
    void SetShadowAtlasRegion(unsigned int index, const glm::vec4& region) {
        if (index < 3) shadow_atlas_region[index] = region;
    }
    void SetSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) {
        if (index < 4) spot_light_space_matrix[index] = mat;
    }
    void SetLightProbeSH(const glm::vec4 sh_in[9], bool enabled) {
        for (int i = 0; i < 9; ++i) light_probe_sh[i] = sh_in[i];
        light_probe_enabled = enabled;
    }
    void SetGBufferTexture(unsigned int index, unsigned int handle) {
        if (index < kMaxGBufferTextures) gbuffer_texture[index] = handle;
    }
    void SetDDGI(bool enabled, unsigned int irradiance_atlas,
                 const glm::vec3& grid_origin, const glm::vec3& grid_spacing,
                 const glm::ivec3& grid_resolution, int irradiance_texels,
                 float gi_intensity, float normal_bias) {
        ddgi_enabled = enabled;
        ddgi_irradiance_atlas = irradiance_atlas;
        ddgi_grid_origin = grid_origin;
        ddgi_grid_spacing = grid_spacing;
        ddgi_grid_resolution = grid_resolution;
        ddgi_irradiance_texels = irradiance_texels;
        ddgi_gi_intensity = gi_intensity;
        ddgi_normal_bias = normal_bias;
    }

    void BeginFrame() {
        current_frame_stats = RenderStats{};
    }
    void EndFrame() {
        last_frame_stats = current_frame_stats;
    }
};

// ============================================================
// UBO 数据准备辅助函数（三端共用，仅负责填充结构体，不涉及 API 上传）
// ============================================================

/// 从 MeshDrawItem 中提取 PerScene UBO 数据
inline PerSceneUBO PreparePerSceneUBO(const MeshDrawItem& item,
                                       const DrawExecutorGlobalState& state) {
    PerSceneUBO scene{};
    const float light_w = (item.lighting_enabled && !state.force_unlit) ? 1.0f : 0.0f;
    scene.light_dir_and_enabled = glm::vec4(item.light_direction, light_w);
    scene.light_color_and_ambient = glm::vec4(
        item.light_color,
        item.ambient_intensity);
    scene.light_params = glm::vec4(
        item.light_intensity,
        item.shadow_strength,
        item.receive_shadow ? 1.0f : 0.0f,
        static_cast<float>(item.shading_mode));
    scene.cascade_splits = glm::vec4(
        state.cascade_splits[0],
        state.cascade_splits[1],
        state.cascade_splits[2],
        static_cast<float>(item.wboit_mode));
    for (int i = 0; i < 3; ++i)
        scene.light_space_matrices[i] = state.light_space_matrix[i];
    for (int i = 0; i < 3; ++i)
        scene.shadow_atlas_regions[i] = state.shadow_atlas_region[i];
    return scene;
}

/// 从 MeshDrawItem 中提取 PerMaterial UBO 数据
inline PerMaterialUBO PreparePerMaterialUBO(const MeshDrawItem& item,
                                            const DrawExecutorGlobalState& state) {
    PerMaterialUBO mat{};
    if (state.overdraw_mode) {
        // Overdraw 可视化：每 fragment 输出固定低强度颜色，
        // 通过 additive blend 叠加后以亮度显示过度绘制
        mat.albedo = glm::vec4(0.1f, 0.04f, 0.02f, 0.0f);
        mat.roughness_ao = glm::vec4(1.0f, 1.0f, 0.0f, 0.0f);
        mat.emissive = glm::vec4(0.0f);
        mat.flags = glm::vec4(0.0f);
    } else {
        mat.albedo = glm::vec4(item.material_albedo, item.material_metallic);
        mat.roughness_ao = glm::vec4(
            item.material_roughness,
            item.material_ao,
            item.material_normal_strength,
            item.material_alpha_cutoff);
        mat.emissive = glm::vec4(
            item.material_emissive,
            item.material_alpha_test ? 1.0f : 0.0f);
        mat.flags = glm::vec4(
            item.normal_map_handle != 0 ? 1.0f : 0.0f,
            item.metallic_roughness_map_handle != 0 ? 1.0f : 0.0f,
            item.emissive_map_handle != 0 ? 1.0f : 0.0f,
            item.occlusion_map_handle != 0 ? 1.0f : 0.0f);
    }
    mat.extra_params = glm::vec4(
        item.material_sss_strength,
        item.material_clear_coat,
        item.material_clear_coat_roughness,
        item.material_anisotropy);
    mat.extra_params2 = glm::vec4(
        item.material_pom_height_scale,
        item.material_sss_tint.x, item.material_sss_tint.y, item.material_sss_tint.z);
    if (item.shading_mode == 5) {
        // Watercolor 模式：复用 toon 槽位传递水彩参数
        mat.toon_shadow_color = glm::vec4(
            item.watercolor_paper_strength, item.watercolor_edge_darkening,
            item.watercolor_color_bleed, item.watercolor_pigment_density);
        mat.toon_params = glm::vec4(0.0f);
    } else {
        mat.toon_shadow_color = glm::vec4(item.toon_shadow_color, item.toon_shadow_threshold);
        mat.toon_params = glm::vec4(
            item.toon_shadow_softness, item.toon_specular_size,
            item.toon_specular_strength, item.toon_rim_strength);
    }
    return mat;
}

/// 从 MeshDrawItem 中提取 PointLights UBO 数据
inline PointLightsUBO PreparePointLightsUBO(const MeshDrawItem& item) {
    PointLightsUBO ubo{};
    const int count = static_cast<int>(
        (std::min)(item.point_lights.size(),
                   static_cast<size_t>(kMaxPointLightsUBO)));
    ubo.u_point_light_count = count;
    for (int i = 0; i < count; ++i) {
        const auto& pl = item.point_lights[i];
        ubo.u_point_lights[i].color = pl.color;
        ubo.u_point_lights[i].intensity = pl.intensity;
        ubo.u_point_lights[i].position = pl.position;
        ubo.u_point_lights[i].radius = pl.radius;
        ubo.u_point_lights[i].cast_shadow = pl.cast_shadow ? 1 : 0;
        ubo.u_point_lights[i].shadow_index = pl.shadow_index;
    }
    return ubo;
}

/// 从 MeshDrawItem 中提取 SpotLights UBO 数据
inline SpotLightsUBO PrepareSpotLightsUBO(const MeshDrawItem& item) {
    SpotLightsUBO ubo{};
    const int count = static_cast<int>(
        (std::min)(item.spot_lights.size(),
                   static_cast<size_t>(kMaxSpotLightsUBO)));
    ubo.u_spot_light_count = count;
    for (int i = 0; i < count; ++i) {
        const auto& sl = item.spot_lights[i];
        ubo.u_spot_lights[i].color = sl.color;
        ubo.u_spot_lights[i].intensity = sl.intensity;
        ubo.u_spot_lights[i].position = sl.position;
        ubo.u_spot_lights[i].radius = sl.radius;
        ubo.u_spot_lights[i].direction = sl.direction;
        ubo.u_spot_lights[i].inner_cone = sl.inner_cone;
        ubo.u_spot_lights[i].outer_cone = sl.outer_cone;
        ubo.u_spot_lights[i].cast_shadow = sl.cast_shadow ? 1 : 0;
        ubo.u_spot_lights[i].shadow_index = sl.shadow_index;
    }
    return ubo;
}

/// 从 DrawExecutorGlobalState 中提取 LightProbeData UBO 数据
inline LightProbeDataUBO PrepareLightProbeUBO(const DrawExecutorGlobalState& state) {
    LightProbeDataUBO ubo{};
    for (int i = 0; i < 9; ++i)
        ubo.sh_coefficients[i] = state.light_probe_sh[i];
    ubo.probe_params = glm::vec4(
        state.light_probe_enabled ? 1.0f : 0.0f,
        0.0f, 0.0f, 0.0f);
    return ubo;
}

/// 更新排序批次统计（每个后端 DrawMeshBatch 入口处调用一次）
/// sorted_draw_calls += items.size()，material_batch_count += 排序后相邻不同 key 的组数
inline void UpdateSortBatchStats(RenderStats& stats,
                                  const std::vector<MeshDrawItem>& items) noexcept {
    stats.sorted_draw_calls += static_cast<int>(items.size());
    if (items.empty()) return;
    int batches = 1;
    uint64_t prev_key = MakeSortKey(items[0]);
    for (size_t i = 1; i < items.size(); ++i) {
        uint64_t key = MakeSortKey(items[i]);
        if (key != prev_key) {
            ++batches;
            prev_key = key;
        }
    }
    stats.material_batch_count += batches;
}

/// 从 DrawExecutorGlobalState 中提取 SpotLightData UBO 数据
inline SpotLightDataUBO PrepareSpotLightDataUBO(const DrawExecutorGlobalState& state) {
    SpotLightDataUBO ubo{};
    for (int i = 0; i < 4; ++i)
        ubo.u_spot_light_space_matrices[i] = state.spot_light_space_matrix[i];
    return ubo;
}

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DRAW_EXECUTOR_COMMON_H
