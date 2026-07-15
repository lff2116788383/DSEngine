/**
 * @file shader_manager_base.h
 * @brief ShaderManager 共享基类 — 管理句柄分配、计数与内置着色器句柄存储
 *
 * GL/DX11/Vulkan ShaderManager 均继承此基类，消除重复的句柄管理逻辑。
 */

#ifndef DSE_RENDER_SHADER_MANAGER_BASE_H
#define DSE_RENDER_SHADER_MANAGER_BASE_H

#include <cstddef>
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

/**
 * @class ShaderManagerBase
 * @brief 着色器管理器共享基类
 *
 * 提供：
 * - 句柄自增分配
 * - 创建/销毁计数
 * - 内置着色器句柄统一存储与访问器
 */
class ShaderManagerBase {
public:
    virtual ~ShaderManagerBase() = default;

    // --- 内置着色器句柄访问器 ---
    ShaderHandle pbr_shader_handle() const { return ShaderHandle{pbr_shader_handle_}; }
    ShaderHandle skybox_shader_handle() const { return ShaderHandle{skybox_shader_handle_}; }
    ShaderHandle sprite_shader_handle() const { return ShaderHandle{sprite_shader_handle_}; }
    ShaderHandle postprocess_shader_handle() const { return ShaderHandle{postprocess_shader_handle_}; }
    ShaderHandle shadow_shader_handle() const { return ShaderHandle{shadow_shader_handle_}; }
    ShaderHandle bloom_extract_shader_handle() const { return ShaderHandle{bloom_extract_shader_handle_}; }
    ShaderHandle bloom_downsample_cs_handle() const { return ShaderHandle{bloom_downsample_cs_handle_}; }
    ShaderHandle bloom_upsample_cs_handle() const { return ShaderHandle{bloom_upsample_cs_handle_}; }
    ShaderHandle bloom_composite_shader_handle() const { return ShaderHandle{bloom_composite_shader_handle_}; }
    ShaderHandle bloom_composite_ssao_shader_handle() const { return ShaderHandle{bloom_composite_ssao_shader_handle_}; }
    ShaderHandle fxaa_shader_handle() const { return ShaderHandle{fxaa_shader_handle_}; }
    ShaderHandle ssao_shader_handle() const { return ShaderHandle{ssao_shader_handle_}; }
    ShaderHandle ssao_blur_shader_handle() const { return ShaderHandle{ssao_blur_shader_handle_}; }
    ShaderHandle ssao_apply_shader_handle() const { return ShaderHandle{ssao_apply_shader_handle_}; }
    ShaderHandle contact_shadow_shader_handle() const { return ShaderHandle{contact_shadow_shader_handle_}; }
    ShaderHandle lum_compute_shader_handle() const { return ShaderHandle{lum_compute_shader_handle_}; }
    ShaderHandle lum_adapt_shader_handle() const { return ShaderHandle{lum_adapt_shader_handle_}; }
    ShaderHandle tonemapping_shader_handle() const { return ShaderHandle{tonemapping_shader_handle_}; }
    ShaderHandle bloom_composite_ssao_ae_shader_handle() const { return ShaderHandle{bloom_composite_ssao_ae_shader_handle_}; }
    ShaderHandle color_grading_shader_handle() const { return ShaderHandle{color_grading_shader_handle_}; }
    ShaderHandle taa_resolve_shader_handle() const { return ShaderHandle{taa_resolve_shader_handle_}; }
    ShaderHandle dof_shader_handle() const { return ShaderHandle{dof_shader_handle_}; }
    ShaderHandle motion_blur_shader_handle() const { return ShaderHandle{motion_blur_shader_handle_}; }
    ShaderHandle ssr_shader_handle() const { return ShaderHandle{ssr_shader_handle_}; }
    ShaderHandle motion_vector_shader_handle() const { return ShaderHandle{motion_vector_shader_handle_}; }
    ShaderHandle gbuffer_shader_handle() const { return ShaderHandle{gbuffer_shader_handle_}; }
    ShaderHandle gbuffer_mesh_shader_handle() const { return ShaderHandle{gbuffer_mesh_shader_handle_}; }
    ShaderHandle deferred_lighting_shader_handle() const { return ShaderHandle{deferred_lighting_shader_handle_}; }
    ShaderHandle edge_detect_shader_handle() const { return ShaderHandle{edge_detect_shader_handle_}; }
    ShaderHandle volumetric_fog_shader_handle() const { return ShaderHandle{volumetric_fog_shader_handle_}; }
    ShaderHandle volumetric_cloud_shader_handle() const { return ShaderHandle{volumetric_cloud_shader_handle_}; }
    ShaderHandle decal_shader_handle() const { return ShaderHandle{decal_shader_handle_}; }
    ShaderHandle wboit_composite_shader_handle() const { return ShaderHandle{wboit_composite_shader_handle_}; }
    ShaderHandle water_shader_handle() const { return ShaderHandle{water_shader_handle_}; }
    ShaderHandle light_shaft_shader_handle() const { return ShaderHandle{light_shaft_shader_handle_}; }
    ShaderHandle gpu_driven_pbr_shader_handle() const { return ShaderHandle{gpu_driven_pbr_shader_handle_}; }
    ShaderHandle gpu_driven_shadow_shader_handle() const { return ShaderHandle{gpu_driven_shadow_shader_handle_}; }
    ShaderHandle atmosphere_transmittance_lut_shader_handle() const { return ShaderHandle{atmosphere_transmittance_lut_shader_handle_}; }
    ShaderHandle atmosphere_sky_shader_handle() const { return ShaderHandle{atmosphere_sky_shader_handle_}; }
    ShaderHandle sss_blur_shader_handle() const { return ShaderHandle{sss_blur_shader_handle_}; }
    ShaderHandle weather_particle_shader_handle() const { return ShaderHandle{weather_particle_shader_handle_}; }
    ShaderHandle eye_shader_handle() const { return ShaderHandle{eye_shader_handle_}; }
    ShaderHandle text_sdf_shader_handle() const { return ShaderHandle{text_sdf_shader_handle_}; }
    ShaderHandle ui_effects_shader_handle() const { return ShaderHandle{ui_effects_shader_handle_}; }

    // --- 统计计数 ---
    std::size_t programs_created() const { return programs_created_; }
    std::size_t programs_destroyed() const { return programs_destroyed_; }

protected:
    /// 分配下一个句柄（子类设置 next_handle_ 初始值以区分后端）
    unsigned int AllocateHandle() { return next_handle_++; }

    unsigned int next_handle_ = 100000;

    // --- 内置着色器句柄 ---
    unsigned int pbr_shader_handle_ = 0;
    unsigned int skybox_shader_handle_ = 0;
    unsigned int sprite_shader_handle_ = 0;
    unsigned int postprocess_shader_handle_ = 0;
    unsigned int shadow_shader_handle_ = 0;
    unsigned int bloom_extract_shader_handle_ = 0;
    unsigned int bloom_downsample_cs_handle_ = 0;
    unsigned int bloom_upsample_cs_handle_ = 0;
    unsigned int bloom_composite_shader_handle_ = 0;
    unsigned int bloom_composite_ssao_shader_handle_ = 0;
    unsigned int fxaa_shader_handle_ = 0;
    unsigned int ssao_shader_handle_ = 0;
    unsigned int ssao_blur_shader_handle_ = 0;
    unsigned int ssao_apply_shader_handle_ = 0;
    unsigned int contact_shadow_shader_handle_ = 0;
    unsigned int lum_compute_shader_handle_ = 0;
    unsigned int lum_adapt_shader_handle_ = 0;
    unsigned int tonemapping_shader_handle_ = 0;
    unsigned int bloom_composite_ssao_ae_shader_handle_ = 0;
    unsigned int color_grading_shader_handle_ = 0;
    unsigned int taa_resolve_shader_handle_ = 0;
    unsigned int dof_shader_handle_ = 0;
    unsigned int motion_blur_shader_handle_ = 0;
    unsigned int ssr_shader_handle_ = 0;
    unsigned int motion_vector_shader_handle_ = 0;
    unsigned int gbuffer_shader_handle_ = 0;
    unsigned int gbuffer_mesh_shader_handle_ = 0;  ///< forward_pbr.vert + gbuffer.frag（MeshRenderer 简单顶点缓冲 GBuffer 路径，阶段4-M3）
    unsigned int deferred_lighting_shader_handle_ = 0;
    unsigned int edge_detect_shader_handle_ = 0;
    unsigned int volumetric_fog_shader_handle_ = 0;
    unsigned int volumetric_cloud_shader_handle_ = 0;
    unsigned int decal_shader_handle_ = 0;
    unsigned int wboit_composite_shader_handle_ = 0;
    unsigned int water_shader_handle_ = 0;
    unsigned int light_shaft_shader_handle_ = 0;
    unsigned int gpu_driven_pbr_shader_handle_ = 0;
    unsigned int gpu_driven_shadow_shader_handle_ = 0;
    unsigned int atmosphere_transmittance_lut_shader_handle_ = 0;
    unsigned int atmosphere_sky_shader_handle_ = 0;
    unsigned int sss_blur_shader_handle_ = 0;
    unsigned int weather_particle_shader_handle_ = 0;
    unsigned int eye_shader_handle_ = 0;
    unsigned int text_sdf_shader_handle_ = 0;
    unsigned int ui_effects_shader_handle_ = 0;

    // --- 计数器 ---
    std::size_t programs_created_ = 0;
    std::size_t programs_destroyed_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_SHADER_MANAGER_BASE_H
