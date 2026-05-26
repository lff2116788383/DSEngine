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

    static constexpr size_t kHiZMaxObjects = 65536;
    unsigned int hiz_texture = 0;        // Hi-Z depth mipmap (R32F, RHI handle)
    dse::render::BufferHandle hiz_visibility_ssbo; // Visibility SSBO for Hi-Z culling
    dse::render::BufferHandle hiz_aabb_ssbo;         // AABB SSBO for Hi-Z culling
    size_t hiz_ssbo_capacity = 0;        // SSBO 当前容量（对象数）
    unsigned int hiz_copy_shader = 0;    // Compute: depth → Hi-Z mip 0
    unsigned int hiz_downsample_shader = 0; // Compute: mip N-1 → mip N
    unsigned int hiz_cull_shader = 0;    // Compute: AABB 過濾

    // --- GPU Driven Rendering ---
    dse::render::BufferHandle gpu_indirect_buffer;       // Indirect draw argument buffer
    dse::render::BufferHandle gpu_instance_ssbo;         // GPUInstanceData[] SSBO
    dse::render::BufferHandle gpu_material_ssbo;         // GPUMaterialData[] SSBO
    dse::render::BufferHandle gpu_visible_indices_ssbo;  // visible instance indices SSBO
    dse::render::BufferHandle gpu_atomic_counter_ssbo;   // atomic draw count SSBO
    dse::render::BufferHandle gpu_draw_cmd_ssbo;         // DrawElementsIndirectCommand[] as SSBO (for compute write)
    dse::render::BufferHandle gpu_aabb_ssbo;
    dse::render::BufferHandle gpu_mega_vbo;              // 统一顶点缓冲区
    dse::render::BufferHandle gpu_mega_ibo;              // 统一索引缓冲区
    dse::render::VertexArrayHandle gpu_mega_vao;              // Mega buffer VAO
    unsigned int gpu_cull_shader = 0;           // GPU Driven culling compute shader
    size_t gpu_aabb_capacity = 0;
    size_t gpu_instance_capacity = 0;           // instance SSBO 当前容量
    size_t gpu_material_capacity = 0;           // material SSBO 当前容量
    size_t gpu_mega_vbo_capacity = 0;           // mega VBO 当前容量（字节）
    size_t gpu_mega_ibo_capacity = 0;           // mega IBO 当前容量（字节）
    bool gpu_driven_supported = false;          // 运行时检测结果

    unsigned int sprite_pipeline_state = 0;
    unsigned int mesh_pipeline_state = 0;
    unsigned int prez_pipeline_state = 0;
    unsigned int composite_pipeline_state = 0;
    unsigned int shadow_render_target[CSM_CASCADES] = {0, 0, 0};
    unsigned int spot_shadow_render_target[4] = {0, 0, 0, 0};
    unsigned int point_shadow_render_target[4] = {0, 0, 0, 0};
    unsigned int rsm_render_target = 0;    // RSM MRT (position+normal+flux, 3 color + depth)
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
        rsm_render_target = 0;
        shadow_pipeline_state = 0;
        decal_blend_pipeline_state = 0;
        wboit_accum_pipeline_state = 0;
        wboit_reveal_pipeline_state = 0;
        hiz_texture = 0;
        hiz_visibility_ssbo = {};
        hiz_aabb_ssbo = {};
        hiz_ssbo_capacity = 0;
        hiz_copy_shader = 0;
        hiz_downsample_shader = 0;
        hiz_cull_shader = 0;
        gpu_indirect_buffer = {};
        gpu_instance_ssbo = {};
        gpu_material_ssbo = {};
        gpu_visible_indices_ssbo = {};
        gpu_atomic_counter_ssbo = {};
        gpu_draw_cmd_ssbo = {};
        gpu_aabb_ssbo = {};
        gpu_mega_vbo = {};
        gpu_mega_ibo = {};
        gpu_mega_vao = {};
        gpu_cull_shader = 0;
        gpu_aabb_capacity = 0;
        gpu_instance_capacity = 0;
        gpu_material_capacity = 0;
        gpu_mega_vbo_capacity = 0;
        gpu_mega_ibo_capacity = 0;
        gpu_driven_supported = false;
    }
};

} // namespace dse::runtime

#endif
