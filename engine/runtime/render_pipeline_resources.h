#ifndef DSE_RENDER_PIPELINE_RESOURCES_H
#define DSE_RENDER_PIPELINE_RESOURCES_H

#include <vector>
#include "engine/render/rhi/rhi_device.h"

namespace dse::runtime {

using dse::render::RenderTargetHandle;
using dse::render::ShaderHandle;
using dse::render::TextureHandle;

struct RenderPipelineResources {
    RenderTargetHandle main_render_target = {};
    RenderTargetHandle scene_render_target = {};
    RenderTargetHandle ui_render_target = {};
    RenderTargetHandle prez_render_target = {};

    RenderTargetHandle pp_bloom_extract_rt = {};
    std::vector<RenderTargetHandle> pp_bloom_mip_rts;

    RenderTargetHandle pp_ssao_rt = {};       // 半分辨率 AO
    RenderTargetHandle pp_ssao_blur_rt = {};  // 模糊后 AO
    RenderTargetHandle pp_contact_shadow_rt = {};  // 接触阴影
    RenderTargetHandle pp_fxaa_rt = {};       // FXAA 输出
    RenderTargetHandle pp_taa_rt = {};        // TAA resolve 输出
    RenderTargetHandle pp_dof_rt = {};        // DOF 输出
    RenderTargetHandle pp_ssr_rt = {};        // SSR 输出
    RenderTargetHandle pp_motion_vector_rt = {}; // Motion Vector (RG16F)
    RenderTargetHandle pp_outline_rt = {};       // Outline / Edge Detection
    RenderTargetHandle pp_fog_rt = {};           // Volumetric Fog
    RenderTargetHandle pp_cloud_rt = {};         // Volumetric Cloud
    RenderTargetHandle wboit_accum_rt = {};      // WBOIT accumulation (RGBA16F)
    RenderTargetHandle wboit_reveal_rt = {};     // WBOIT revealage (RGBA16F)

    RenderTargetHandle pp_sss_temp_rt = {};        // SSS blur intermediate (RGBA16F)

    RenderTargetHandle pp_lum_temp_rt = {};     // 64x64 log luminance
    RenderTargetHandle pp_lum_adapted_rt[2]{}; // 1x1 ping-pong (EMA adapted exposure)

    static constexpr size_t kHiZMaxObjects = 65536;
    TextureHandle hiz_texture = {};        // Hi-Z depth mipmap (R32F, RHI handle)
    dse::render::BufferHandle hiz_visibility_ssbo; // Visibility SSBO for Hi-Z culling
    dse::render::BufferHandle hiz_aabb_ssbo;         // AABB SSBO for Hi-Z culling
    size_t hiz_ssbo_capacity = 0;        // SSBO 当前容量（对象数）
    ShaderHandle hiz_copy_shader = {};    // Compute: depth → Hi-Z mip 0
    ShaderHandle hiz_downsample_shader = {}; // Compute: mip N-1 → mip N
    ShaderHandle hiz_cull_shader = {};    // Compute: AABB 過濾

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
    ShaderHandle gpu_cull_shader = {};           // GPU Driven culling compute shader
    size_t gpu_aabb_capacity = 0;
    size_t gpu_instance_capacity = 0;           // instance SSBO 当前容量
    size_t gpu_material_capacity = 0;           // material SSBO 当前容量
    size_t gpu_mega_vbo_capacity = 0;           // mega VBO 当前容量（字节）
    size_t gpu_mega_ibo_capacity = 0;           // mega IBO 当前容量（字节）
    bool gpu_driven_supported = false;          // 运行时检测结果

    dse::render::PipelineHandle sprite_pipeline_state;
    dse::render::PipelineHandle mesh_pipeline_state;
    dse::render::PipelineHandle prez_pipeline_state;
    dse::render::PipelineHandle composite_pipeline_state;
    RenderTargetHandle shadow_render_target[CSM_CASCADES]{};
    RenderTargetHandle shadow_atlas_render_target = {};  ///< CSM shadow atlas (4096×2048 depth-only)
    RenderTargetHandle spot_shadow_render_target[4]{};
    RenderTargetHandle point_shadow_render_target[4]{};
    RenderTargetHandle rsm_render_target = {};    // RSM MRT (position+normal+flux, 3 color + depth)
    dse::render::PipelineHandle shadow_pipeline_state;
    dse::render::PipelineHandle decal_blend_pipeline_state;
    dse::render::PipelineHandle wboit_accum_pipeline_state;
    dse::render::PipelineHandle wboit_reveal_pipeline_state;

    void Reset() {
        main_render_target = {};
        scene_render_target = {};
        ui_render_target = {};
        prez_render_target = {};
        for (int i = 0; i < CSM_CASCADES; ++i) {
            shadow_render_target[i] = {};
        }
        for (int i = 0; i < 4; ++i) {
            spot_shadow_render_target[i] = {};
            point_shadow_render_target[i] = {};
        }
        pp_bloom_extract_rt = {};
        pp_bloom_mip_rts.clear();
        pp_ssao_rt = {};
        pp_ssao_blur_rt = {};
        pp_contact_shadow_rt = {};
        pp_fxaa_rt = {};
        pp_taa_rt = {};
        pp_dof_rt = {};
        pp_ssr_rt = {};
        pp_motion_vector_rt = {};
        pp_outline_rt = {};
        pp_fog_rt = {};
        pp_cloud_rt = {};
        wboit_accum_rt = {};
        wboit_reveal_rt = {};
        pp_sss_temp_rt = {};
        pp_lum_temp_rt = {};
        pp_lum_adapted_rt[0] = {};
        pp_lum_adapted_rt[1] = {};
        sprite_pipeline_state = {};
        mesh_pipeline_state = {};
        prez_pipeline_state = {};
        composite_pipeline_state = {};
        rsm_render_target = {};
        shadow_pipeline_state = {};
        decal_blend_pipeline_state = {};
        wboit_accum_pipeline_state = {};
        wboit_reveal_pipeline_state = {};
        hiz_texture = {};
        hiz_visibility_ssbo = {};
        hiz_aabb_ssbo = {};
        hiz_ssbo_capacity = 0;
        hiz_copy_shader = {};
        hiz_downsample_shader = {};
        hiz_cull_shader = {};
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
        gpu_cull_shader = {};
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
