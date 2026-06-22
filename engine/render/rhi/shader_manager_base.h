/**
 * @file shader_manager_base.h
 * @brief ShaderManager 共享基类 — 管理句柄分配、计数与内置着色器句柄存储
 *
 * GL/DX11/Vulkan ShaderManager 均继承此基类，消除重复的句柄管理逻辑。
 */

#ifndef DSE_RENDER_SHADER_MANAGER_BASE_H
#define DSE_RENDER_SHADER_MANAGER_BASE_H

#include <cstddef>

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
    unsigned int pbr_shader_handle() const { return pbr_shader_handle_; }
    unsigned int skybox_shader_handle() const { return skybox_shader_handle_; }
    unsigned int sprite_shader_handle() const { return sprite_shader_handle_; }
    unsigned int postprocess_shader_handle() const { return postprocess_shader_handle_; }
    unsigned int shadow_shader_handle() const { return shadow_shader_handle_; }
    unsigned int bloom_extract_shader_handle() const { return bloom_extract_shader_handle_; }
    unsigned int bloom_downsample_cs_handle() const { return bloom_downsample_cs_handle_; }
    unsigned int bloom_upsample_cs_handle() const { return bloom_upsample_cs_handle_; }
    unsigned int bloom_composite_shader_handle() const { return bloom_composite_shader_handle_; }
    unsigned int bloom_composite_ssao_shader_handle() const { return bloom_composite_ssao_shader_handle_; }
    unsigned int fxaa_shader_handle() const { return fxaa_shader_handle_; }
    unsigned int ssao_shader_handle() const { return ssao_shader_handle_; }
    unsigned int ssao_blur_shader_handle() const { return ssao_blur_shader_handle_; }
    unsigned int ssao_apply_shader_handle() const { return ssao_apply_shader_handle_; }
    unsigned int contact_shadow_shader_handle() const { return contact_shadow_shader_handle_; }
    unsigned int lum_compute_shader_handle() const { return lum_compute_shader_handle_; }
    unsigned int lum_adapt_shader_handle() const { return lum_adapt_shader_handle_; }
    unsigned int tonemapping_shader_handle() const { return tonemapping_shader_handle_; }
    unsigned int bloom_composite_ssao_ae_shader_handle() const { return bloom_composite_ssao_ae_shader_handle_; }
    unsigned int color_grading_shader_handle() const { return color_grading_shader_handle_; }
    unsigned int taa_resolve_shader_handle() const { return taa_resolve_shader_handle_; }
    unsigned int dof_shader_handle() const { return dof_shader_handle_; }
    unsigned int motion_blur_shader_handle() const { return motion_blur_shader_handle_; }
    unsigned int ssr_shader_handle() const { return ssr_shader_handle_; }
    unsigned int motion_vector_shader_handle() const { return motion_vector_shader_handle_; }
    unsigned int gbuffer_shader_handle() const { return gbuffer_shader_handle_; }
    unsigned int gbuffer_mesh_shader_handle() const { return gbuffer_mesh_shader_handle_; }
    unsigned int deferred_lighting_shader_handle() const { return deferred_lighting_shader_handle_; }
    unsigned int edge_detect_shader_handle() const { return edge_detect_shader_handle_; }
    unsigned int volumetric_fog_shader_handle() const { return volumetric_fog_shader_handle_; }
    unsigned int volumetric_cloud_shader_handle() const { return volumetric_cloud_shader_handle_; }
    unsigned int decal_shader_handle() const { return decal_shader_handle_; }
    unsigned int wboit_composite_shader_handle() const { return wboit_composite_shader_handle_; }
    unsigned int water_shader_handle() const { return water_shader_handle_; }
    unsigned int light_shaft_shader_handle() const { return light_shaft_shader_handle_; }
    unsigned int gpu_driven_pbr_shader_handle() const { return gpu_driven_pbr_shader_handle_; }
    unsigned int gpu_driven_shadow_shader_handle() const { return gpu_driven_shadow_shader_handle_; }
    unsigned int atmosphere_transmittance_lut_shader_handle() const { return atmosphere_transmittance_lut_shader_handle_; }
    unsigned int atmosphere_sky_shader_handle() const { return atmosphere_sky_shader_handle_; }
    unsigned int sss_blur_shader_handle() const { return sss_blur_shader_handle_; }
    unsigned int weather_particle_shader_handle() const { return weather_particle_shader_handle_; }
    unsigned int eye_shader_handle() const { return eye_shader_handle_; }
    unsigned int text_sdf_shader_handle() const { return text_sdf_shader_handle_; }
    unsigned int ui_effects_shader_handle() const { return ui_effects_shader_handle_; }

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
