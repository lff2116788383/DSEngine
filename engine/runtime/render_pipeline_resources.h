#ifndef DSE_RENDER_PIPELINE_RESOURCES_H
#define DSE_RENDER_PIPELINE_RESOURCES_H

#include <vector>
#include "engine/render/rhi/rhi_device.h"

namespace dse::runtime {

struct RenderPipelineResources {
    unsigned int main_render_target = 0;
    unsigned int scene_render_target = 0;
    unsigned int ui_render_target = 0;
    unsigned int prez_render_target = 0;

    unsigned int pp_bloom_extract_rt = 0;
    std::vector<unsigned int> pp_bloom_mip_rts;

    unsigned int pp_ssao_rt = 0;       // 半分辨率 AO
    unsigned int pp_ssao_blur_rt = 0;  // 模糊后 AO
    unsigned int pp_contact_shadow_rt = 0;  // 接触阴影
    unsigned int pp_fxaa_rt = 0;       // FXAA 输出
    unsigned int pp_taa_rt = 0;        // TAA resolve 输出
    unsigned int pp_dof_rt = 0;        // DOF 输出
    unsigned int pp_ssr_rt = 0;        // SSR 输出
    unsigned int pp_motion_vector_rt = 0; // Motion Vector (RG16F)
    unsigned int pp_outline_rt = 0;       // Outline / Edge Detection
    unsigned int pp_fog_rt = 0;           // Volumetric Fog
    unsigned int wboit_accum_rt = 0;      // WBOIT accumulation (RGBA16F)
    unsigned int wboit_reveal_rt = 0;     // WBOIT revealage (RGBA16F)

    unsigned int gbuffer_rt = 0;            // GBuffer MRT (3 color + depth)
    unsigned int deferred_lighting_rt = 0;   // Deferred lighting output

    unsigned int pp_lum_temp_rt = 0;     // 64x64 log luminance
    unsigned int pp_lum_adapted_rt[2] = {0, 0}; // 1x1 ping-pong (EMA adapted exposure)

    static constexpr size_t kHiZMaxObjects = 8192;
    unsigned int hiz_texture = 0;        // Hi-Z depth mipmap (R32F, RHI handle)
    unsigned int hiz_visibility_ssbo = 0; // Visibility SSBO for Hi-Z culling
    unsigned int hiz_aabb_ssbo = 0;       // AABB SSBO for Hi-Z culling
    size_t hiz_ssbo_capacity = 0;        // SSBO 当前容量（对象数）
    unsigned int hiz_copy_shader = 0;    // Compute: depth → Hi-Z mip 0
    unsigned int hiz_downsample_shader = 0; // Compute: mip N-1 → mip N
    unsigned int hiz_cull_shader = 0;    // Compute: AABB 過滤

    unsigned int sprite_pipeline_state = 0;
    unsigned int mesh_pipeline_state = 0;
    unsigned int prez_pipeline_state = 0;
    unsigned int composite_pipeline_state = 0;
    unsigned int shadow_render_target[CSM_CASCADES] = {0, 0, 0};
    unsigned int spot_shadow_render_target[4] = {0, 0, 0, 0};
    unsigned int point_shadow_render_target[4] = {0, 0, 0, 0};
    unsigned int shadow_pipeline_state = 0;
    unsigned int decal_blend_pipeline_state = 0;
    unsigned int wboit_accum_pipeline_state = 0;
    unsigned int wboit_reveal_pipeline_state = 0;

    void Reset() {
        main_render_target = 0;
        scene_render_target = 0;
        ui_render_target = 0;
        prez_render_target = 0;
        for (int i = 0; i < CSM_CASCADES; ++i) {
            shadow_render_target[i] = 0;
        }
        for (int i = 0; i < 4; ++i) {
            spot_shadow_render_target[i] = 0;
            point_shadow_render_target[i] = 0;
        }
        pp_bloom_extract_rt = 0;
        pp_bloom_mip_rts.clear();
        pp_ssao_rt = 0;
        pp_ssao_blur_rt = 0;
        pp_contact_shadow_rt = 0;
        pp_fxaa_rt = 0;
        pp_taa_rt = 0;
        pp_dof_rt = 0;
        pp_ssr_rt = 0;
        pp_motion_vector_rt = 0;
        pp_outline_rt = 0;
        pp_fog_rt = 0;
        wboit_accum_rt = 0;
        wboit_reveal_rt = 0;
        gbuffer_rt = 0;
        deferred_lighting_rt = 0;
        pp_lum_temp_rt = 0;
        pp_lum_adapted_rt[0] = 0;
        pp_lum_adapted_rt[1] = 0;
        sprite_pipeline_state = 0;
        mesh_pipeline_state = 0;
        prez_pipeline_state = 0;
        composite_pipeline_state = 0;
        shadow_pipeline_state = 0;
        decal_blend_pipeline_state = 0;
        wboit_accum_pipeline_state = 0;
        wboit_reveal_pipeline_state = 0;
        hiz_texture = 0;
        hiz_visibility_ssbo = 0;
        hiz_aabb_ssbo = 0;
        hiz_ssbo_capacity = 0;
        hiz_copy_shader = 0;
        hiz_downsample_shader = 0;
        hiz_cull_shader = 0;
    }
};

} // namespace dse::runtime

#endif
