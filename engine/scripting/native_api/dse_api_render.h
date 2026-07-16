/**
 * @file dse_api_render.h
 * @brief DSEngine Native C ABI — Rendering (Camera/Mesh/Light/PostProcess/Particles) module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef DSE_API_RENDER_H
#define DSE_API_RENDER_H

#include <stdint.h>

#ifndef DSE_CAPI
#  ifdef _WIN32
#    define DSE_CAPI __declspec(dllexport)
#  else
#    define DSE_CAPI __attribute__((visibility("default")))
#  endif
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Camera3DComponent
// ============================================================

DSE_CAPI void  dse_camera3d_add(uint32_t e, float fov, float near_clip, float far_clip);
DSE_CAPI float dse_camera3d_get_fov(uint32_t e);
DSE_CAPI void  dse_camera3d_set_fov(uint32_t e, float v);
DSE_CAPI float dse_camera3d_get_near_clip(uint32_t e);
DSE_CAPI void  dse_camera3d_set_near_clip(uint32_t e, float v);
DSE_CAPI float dse_camera3d_get_far_clip(uint32_t e);
DSE_CAPI void  dse_camera3d_set_far_clip(uint32_t e, float v);
DSE_CAPI int   dse_camera3d_get_enabled(uint32_t e);
DSE_CAPI void  dse_camera3d_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_camera3d_get_priority(uint32_t e);
DSE_CAPI void  dse_camera3d_set_priority(uint32_t e, int v);

// ============================================================

// MeshRendererComponent
// ============================================================

DSE_CAPI void  dse_mesh_renderer_add(uint32_t e, const char* mesh_path);
// 手写 setter（capi_setter:manual）：设 mesh_path 并清空过程网格缓存；getter 由 codegen 生成
DSE_CAPI void  dse_mesh_renderer_set_mesh_path(uint32_t e, const char* mesh_path);
DSE_CAPI int   dse_mesh_renderer_get_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void  dse_mesh_renderer_set_shader_variant(uint32_t e, const char* v);
DSE_CAPI int   dse_mesh_renderer_get_shader_variant(uint32_t e, char* buf, int buf_size);
DSE_CAPI void  dse_mesh_renderer_get_color(uint32_t e, float* r, float* g, float* b, float* a);
DSE_CAPI void  dse_mesh_renderer_set_color(uint32_t e, float r, float g, float b, float a);
DSE_CAPI int   dse_mesh_renderer_get_visible(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_visible(uint32_t e, int v);
DSE_CAPI float dse_mesh_renderer_get_metallic(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_metallic(uint32_t e, float v);
DSE_CAPI float dse_mesh_renderer_get_roughness(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_roughness(uint32_t e, float v);
DSE_CAPI void  dse_mesh_renderer_get_emissive(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_mesh_renderer_set_emissive(uint32_t e, float r, float g, float b);
DSE_CAPI int   dse_mesh_renderer_get_receive_shadow(uint32_t e);
DSE_CAPI void  dse_mesh_renderer_set_receive_shadow(uint32_t e, int v);

// ============================================================

// DirectionalLight3DComponent
// ============================================================

DSE_CAPI void  dse_dir_light_add(uint32_t e);
DSE_CAPI void  dse_dir_light_get_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_dir_light_set_direction(uint32_t e, float x, float y, float z);
DSE_CAPI void  dse_dir_light_get_color(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_dir_light_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI float dse_dir_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_dir_light_set_intensity(uint32_t e, float v);
DSE_CAPI float dse_dir_light_get_ambient_intensity(uint32_t e);
DSE_CAPI void  dse_dir_light_set_ambient_intensity(uint32_t e, float v);
DSE_CAPI int   dse_dir_light_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_dir_light_set_cast_shadow(uint32_t e, int v);
DSE_CAPI float dse_dir_light_get_shadow_strength(uint32_t e);
DSE_CAPI void  dse_dir_light_set_shadow_strength(uint32_t e, float v);
DSE_CAPI int   dse_dir_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_dir_light_set_enabled(uint32_t e, int v);
// S1.8 Tier C：复合阴影参数 setter，封装 cascade 级联约束（split[i] ≥ split[i-1]+0.1）+ clamp；
//             手写实现见 dse_api.cpp（非 codegen）。传入值由调用方合并好现值。
DSE_CAPI void  dse_dir_light_set_shadow_params(uint32_t e, int cast_shadow, float shadow_strength,
                                               float c0, float c1, float c2, float lambda);

// ============================================================

// PointLightComponent
// ============================================================

DSE_CAPI void  dse_point_light_add(uint32_t e);
DSE_CAPI void  dse_point_light_get_color(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_point_light_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI float dse_point_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_point_light_set_intensity(uint32_t e, float v);
DSE_CAPI float dse_point_light_get_radius(uint32_t e);
DSE_CAPI void  dse_point_light_set_radius(uint32_t e, float v);
DSE_CAPI int   dse_point_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_point_light_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_point_light_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_point_light_set_cast_shadow(uint32_t e, int v);

// ============================================================

// SpotLightComponent
// ============================================================

DSE_CAPI void  dse_spot_light_add(uint32_t e);
DSE_CAPI void  dse_spot_light_get_color(uint32_t e, float* r, float* g, float* b);
DSE_CAPI void  dse_spot_light_set_color(uint32_t e, float r, float g, float b);
DSE_CAPI float dse_spot_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_spot_light_set_intensity(uint32_t e, float v);
DSE_CAPI float dse_spot_light_get_radius(uint32_t e);
DSE_CAPI void  dse_spot_light_set_radius(uint32_t e, float v);
DSE_CAPI float dse_spot_light_get_inner_cone_angle(uint32_t e);
DSE_CAPI void  dse_spot_light_set_inner_cone_angle(uint32_t e, float v);
DSE_CAPI float dse_spot_light_get_outer_cone_angle(uint32_t e);
DSE_CAPI void  dse_spot_light_set_outer_cone_angle(uint32_t e, float v);
DSE_CAPI void  dse_spot_light_get_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_spot_light_set_direction(uint32_t e, float x, float y, float z);
DSE_CAPI int   dse_spot_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_spot_light_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_spot_light_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_spot_light_set_cast_shadow(uint32_t e, int v);

// ============================================================

// SkyLightComponent
// ============================================================

DSE_CAPI void  dse_sky_light_add(uint32_t e);
DSE_CAPI void  dse_sky_light_get_up_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_sky_light_set_up_color(uint32_t e, float x, float y, float z);
DSE_CAPI void  dse_sky_light_get_down_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_sky_light_set_down_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_sky_light_get_intensity(uint32_t e);
DSE_CAPI void  dse_sky_light_set_intensity(uint32_t e, float v);
DSE_CAPI int   dse_sky_light_get_enabled(uint32_t e);
DSE_CAPI void  dse_sky_light_set_enabled(uint32_t e, int v);

// ============================================================

// TreeComponent
// ============================================================

DSE_CAPI int   dse_tree_get_enabled(uint32_t e);
DSE_CAPI void  dse_tree_set_enabled(uint32_t e, int v);
DSE_CAPI float dse_tree_get_density(uint32_t e);
DSE_CAPI void  dse_tree_set_density(uint32_t e, float v);
DSE_CAPI float dse_tree_get_spawn_radius(uint32_t e);
DSE_CAPI void  dse_tree_set_spawn_radius(uint32_t e, float v);
DSE_CAPI float dse_tree_get_chunk_size(uint32_t e);
DSE_CAPI void  dse_tree_set_chunk_size(uint32_t e, float v);
DSE_CAPI float dse_tree_get_min_scale(uint32_t e);
DSE_CAPI void  dse_tree_set_min_scale(uint32_t e, float v);
DSE_CAPI float dse_tree_get_max_scale(uint32_t e);
DSE_CAPI void  dse_tree_set_max_scale(uint32_t e, float v);
DSE_CAPI float dse_tree_get_lod1_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_lod1_distance(uint32_t e, float v);
DSE_CAPI float dse_tree_get_cull_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_cull_distance(uint32_t e, float v);
DSE_CAPI float dse_tree_get_wind_strength(uint32_t e);
DSE_CAPI void  dse_tree_set_wind_strength(uint32_t e, float v);
DSE_CAPI float dse_tree_get_wind_speed(uint32_t e);
DSE_CAPI void  dse_tree_set_wind_speed(uint32_t e, float v);
DSE_CAPI int   dse_tree_get_cast_shadow(uint32_t e);
DSE_CAPI void  dse_tree_set_cast_shadow(uint32_t e, int v);
DSE_CAPI float dse_tree_get_shadow_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_shadow_distance(uint32_t e, float v);
DSE_CAPI int   dse_tree_get_seed(uint32_t e);
DSE_CAPI void  dse_tree_set_seed(uint32_t e, int v);
DSE_CAPI float dse_tree_get_height_variation(uint32_t e);
DSE_CAPI void  dse_tree_set_height_variation(uint32_t e, float v);
DSE_CAPI int   dse_tree_get_random_rotation(uint32_t e);
DSE_CAPI void  dse_tree_set_random_rotation(uint32_t e, int v);
DSE_CAPI float dse_tree_get_billboard_distance(uint32_t e);
DSE_CAPI void  dse_tree_set_billboard_distance(uint32_t e, float v);

// ============================================================

// TreeComponent — string paths（实现见 dse_api.gen.cpp）
// ============================================================

DSE_CAPI void dse_tree_set_mesh_path(uint32_t e, const char* path);
DSE_CAPI int  dse_tree_get_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void dse_tree_set_lod1_mesh_path(uint32_t e, const char* path);
DSE_CAPI int  dse_tree_get_lod1_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void dse_tree_set_billboard_texture_path(uint32_t e, const char* path);
DSE_CAPI int  dse_tree_get_billboard_texture_path(uint32_t e, char* buf, int buf_size);

// ============================================================

// PostProcessComponent — 每字段访问器（实现见 dse_api_post_process.gen.cpp）
// ============================================================

DSE_CAPI int  dse_post_process_get_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_enabled(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_bloom_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_bloom_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_bloom_threshold(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_threshold(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_bloom_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_intensity(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_bloom_knee(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_knee(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_bloom_mip_weight(uint32_t e);
DSE_CAPI void  dse_post_process_set_bloom_mip_weight(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_color_grading_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_color_grading_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_exposure(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_gamma(uint32_t e);
DSE_CAPI void  dse_post_process_set_gamma(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_ssao_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_ssao_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssao_radius(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_radius(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssao_bias(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_bias(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_ssao_sample_count(uint32_t e);
DSE_CAPI void dse_post_process_set_ssao_sample_count(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssao_power(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_power(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssao_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssao_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_auto_exposure_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_auto_exposure_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_exposure_min(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure_min(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_exposure_max(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure_max(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_adaptation_speed_up(uint32_t e);
DSE_CAPI void  dse_post_process_set_adaptation_speed_up(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_adaptation_speed_down(uint32_t e);
DSE_CAPI void  dse_post_process_set_adaptation_speed_down(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_exposure_compensation(uint32_t e);
DSE_CAPI void  dse_post_process_set_exposure_compensation(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_color_lut_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_color_lut_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_vignette_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_vignette_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_vignette_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_vignette_intensity(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_vignette_radius(uint32_t e);
DSE_CAPI void  dse_post_process_set_vignette_radius(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_vignette_softness(uint32_t e);
DSE_CAPI void  dse_post_process_set_vignette_softness(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_film_grain_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_film_grain_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_film_grain_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_film_grain_intensity(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_film_grain_time_scale(uint32_t e);
DSE_CAPI void  dse_post_process_set_film_grain_time_scale(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_fxaa_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_fxaa_enabled(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_taa_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_taa_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_taa_blend_factor(uint32_t e);
DSE_CAPI void  dse_post_process_set_taa_blend_factor(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_contact_shadow_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_contact_shadow_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_contact_shadow_strength(uint32_t e);
DSE_CAPI void  dse_post_process_set_contact_shadow_strength(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_contact_shadow_steps(uint32_t e);
DSE_CAPI void dse_post_process_set_contact_shadow_steps(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_contact_shadow_step_size(uint32_t e);
DSE_CAPI void  dse_post_process_set_contact_shadow_step_size(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_dof_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_dof_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_dof_focus_distance(uint32_t e);
DSE_CAPI void  dse_post_process_set_dof_focus_distance(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_dof_focus_range(uint32_t e);
DSE_CAPI void  dse_post_process_set_dof_focus_range(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_dof_bokeh_radius(uint32_t e);
DSE_CAPI void  dse_post_process_set_dof_bokeh_radius(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_motion_blur_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_motion_blur_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_motion_blur_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_motion_blur_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_motion_blur_samples(uint32_t e);
DSE_CAPI void dse_post_process_set_motion_blur_samples(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_ssr_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_ssr_enabled(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssr_max_distance(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_max_distance(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssr_thickness(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_thickness(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssr_step_size(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_step_size(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_ssr_max_steps(uint32_t e);
DSE_CAPI void dse_post_process_set_ssr_max_steps(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_ssr_fade_distance(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_fade_distance(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_ssr_max_roughness(uint32_t e);
DSE_CAPI void  dse_post_process_set_ssr_max_roughness(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_outline_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_outline_enabled(uint32_t e, int v);
DSE_CAPI void dse_post_process_get_outline_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_post_process_set_outline_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_post_process_get_outline_thickness(uint32_t e);
DSE_CAPI void  dse_post_process_set_outline_thickness(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_outline_depth_threshold(uint32_t e);
DSE_CAPI void  dse_post_process_set_outline_depth_threshold(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_outline_normal_threshold(uint32_t e);
DSE_CAPI void  dse_post_process_set_outline_normal_threshold(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_light_shaft_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_light_shaft_enabled(uint32_t e, int v);
DSE_CAPI void dse_post_process_get_light_shaft_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_post_process_set_light_shaft_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_post_process_get_light_shaft_density(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_density(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_weight(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_weight(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_decay(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_decay(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_exposure(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_exposure(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_light_shaft_intensity(uint32_t e);
DSE_CAPI void  dse_post_process_set_light_shaft_intensity(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_light_shaft_samples(uint32_t e);
DSE_CAPI void dse_post_process_set_light_shaft_samples(uint32_t e, int v);
DSE_CAPI int  dse_post_process_get_fog_enabled(uint32_t e);
DSE_CAPI void dse_post_process_set_fog_enabled(uint32_t e, int v);
DSE_CAPI void dse_post_process_get_fog_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_post_process_set_fog_color(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_post_process_get_fog_density(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_density(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_height_falloff(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_height_falloff(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_height_offset(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_height_offset(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_start(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_start(uint32_t e, float v);
DSE_CAPI float dse_post_process_get_fog_end(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_end(uint32_t e, float v);
DSE_CAPI int  dse_post_process_get_fog_steps(uint32_t e);
DSE_CAPI void dse_post_process_set_fog_steps(uint32_t e, int v);
DSE_CAPI float dse_post_process_get_fog_sun_scatter(uint32_t e);
DSE_CAPI void  dse_post_process_set_fog_sun_scatter(uint32_t e, float v);

// ============================================================

// Animator3DComponent — S1.9 每字段访问器（实现见 dse_api_animator3d.gen.cpp）
// danim_path/dskel_path 为纯字符串字段：动画系统按路径值比较自动重载，setter 纯赋值无副作用。
// 复合/FSM/blend tree 仍手写于 lua_binding_ecs_animation.cpp，不在此。
// ============================================================
DSE_CAPI int   dse_animator3d_get_enabled(uint32_t e);
DSE_CAPI void  dse_animator3d_set_enabled(uint32_t e, int v);
DSE_CAPI void  dse_animator3d_set_danim_path(uint32_t e, const char* v);
DSE_CAPI int   dse_animator3d_get_danim_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI void  dse_animator3d_set_dskel_path(uint32_t e, const char* v);
DSE_CAPI int   dse_animator3d_get_dskel_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI float dse_animator3d_get_speed(uint32_t e);
DSE_CAPI void  dse_animator3d_set_speed(uint32_t e, float v);
DSE_CAPI int   dse_animator3d_get_loop(uint32_t e);
DSE_CAPI void  dse_animator3d_set_loop(uint32_t e, int v);
DSE_CAPI int   dse_animator3d_get_use_anim_tree(uint32_t e);
DSE_CAPI void  dse_animator3d_set_use_anim_tree(uint32_t e, int v);
DSE_CAPI void  dse_animator3d_set_blend_parameter(uint32_t e, const char* v);
DSE_CAPI int   dse_animator3d_get_blend_parameter(uint32_t e, char* buf, int buf_size);
DSE_CAPI float dse_animator3d_get_blend_parameter_value(uint32_t e);
DSE_CAPI void  dse_animator3d_set_blend_parameter_value(uint32_t e, float v);

// ============================================================
// Physics3D 服务（L5，手写 dse_api_physics3d.cpp）
// 依赖 Physics3D 服务 + ECS 碰撞体回退，非纯组件字段，codegen 无法表达。
// raycast：direction 内部归一化；命中返回 1 并填充非空 out_*（point/normal 为 float[3]）。
// ============================================================
DSE_CAPI int dse_physics3d_raycast(float ox, float oy, float oz,
                                   float dx, float dy, float dz,
                                   float max_dist,
                                   uint32_t* out_entity,
                                   float* out_point,
                                   float* out_normal,
                                   float* out_distance);

// RigidBody3D 动力学（服务委托 + 组件缓存回退）。
// add_force/add_impulse/add_torque/set_angular_velocity：仅在物理服务存在时生效，否则 no-op。
// set_velocity/set_gravity：委托服务（若有）并同步组件缓存。
// get_velocity：服务优先，否则回退组件缓存；get_angular_velocity：服务优先，否则 0。
DSE_CAPI void dse_rigidbody3d_add_force(uint32_t e, float fx, float fy, float fz);
DSE_CAPI void dse_rigidbody3d_add_impulse(uint32_t e, float ix, float iy, float iz);
DSE_CAPI void dse_rigidbody3d_add_torque(uint32_t e, float tx, float ty, float tz);
DSE_CAPI void dse_rigidbody3d_set_velocity(uint32_t e, float vx, float vy, float vz);
DSE_CAPI void dse_rigidbody3d_get_velocity(uint32_t e, float* out_vel);          // float[3]
DSE_CAPI void dse_rigidbody3d_set_angular_velocity(uint32_t e, float ax, float ay, float az);
DSE_CAPI void dse_rigidbody3d_get_angular_velocity(uint32_t e, float* out_vel);  // float[3]
DSE_CAPI void dse_rigidbody3d_set_gravity(uint32_t e, int enabled);

// CharacterController3D。move：服务优先（含 ECS 地形贴地补正），否则 ECS 回退
// （碰撞体推开 + 地形贴地 + 着地检测）。返回 is_grounded(0/1)，填充非空 out_velocity[3] 与 out_flags。
// jump：仅在物理服务存在时生效，返回 success(0/1)。
DSE_CAPI int dse_character_controller3d_move(uint32_t e, float dx, float dy, float dz,
                                             float min_dist, float dt,
                                             float* out_velocity, uint32_t* out_flags);
DSE_CAPI int dse_character_controller3d_jump(uint32_t e, float jump_speed);
// is_grounded/get_position：服务优先，否则 ECS 回退（组件缓存 / Transform）。
DSE_CAPI int dse_character_controller3d_is_grounded(uint32_t e);
DSE_CAPI int dse_character_controller3d_get_position(uint32_t e, float* out_xyz); // float[3]，返回 found(0/1)

// 组件创建（L3，ECS emplace_or_replace）。type/direction 为对应枚举的整型值。
DSE_CAPI void dse_rigidbody3d_add(uint32_t e, int type, float mass);
DSE_CAPI void dse_box_collider3d_add(uint32_t e, float x, float y, float z);
DSE_CAPI void dse_sphere_collider3d_add(uint32_t e, float radius);
DSE_CAPI void dse_capsule_collider3d_add(uint32_t e, float radius, float height,
                                         int direction, int is_trigger);
DSE_CAPI void dse_mesh_collider3d_add(uint32_t e, int convex, int is_trigger);
DSE_CAPI void dse_character_controller3d_add(uint32_t e, float radius, float height,
                                             float slope_limit, float step_offset);
DSE_CAPI void dse_joint3d_add(uint32_t e, uint32_t connected_id, int type,
                              float ax, float ay, float az,
                              float bx, float by, float bz,
                              float break_force, float break_torque);
DSE_CAPI void dse_terrain_heightmap_add(uint32_t e, float origin_x, float origin_z,
                                        float block_size, int cols, int rows,
                                        float scale, int flip_z);

// TerrainHeightmap 数据写入 / 查询。
DSE_CAPI void dse_terrain_heightmap_set_data(uint32_t e, const float* heights, int count);
DSE_CAPI int dse_terrain_get_height(float world_x, float world_z, float* out_y); // 返回 found(0/1)

// Joint3D 附加参数 setter / 查询。
DSE_CAPI void dse_joint3d_set_hinge_limits(uint32_t e, float lower_deg, float upper_deg);
DSE_CAPI void dse_joint3d_set_spring(uint32_t e, float stiffness, float damping);
DSE_CAPI void dse_joint3d_set_distance(uint32_t e, float min_dist, float max_dist);
DSE_CAPI int dse_joint3d_is_broken(uint32_t e);

// 碰撞层 / trigger / 材质。set_layer 写 RigidBody 并委托物理服务（若有）。
// set_trigger/set_material 写入实体上存在的任意碰撞体类型（Box/Sphere/Capsule/Mesh）。
DSE_CAPI void dse_collision_set_layer(uint32_t e, int layer, int mask);
DSE_CAPI void dse_collider_set_trigger(uint32_t e, int is_trigger);
DSE_CAPI void dse_collider_set_material(uint32_t e, float friction, float bounciness);

// 重叠查询：写入命中实体 id 到 out（容量 cap），返回命中总数（可能 > cap）。
DSE_CAPI int dse_physics3d_overlap_sphere(float cx, float cy, float cz, float radius,
                                          uint32_t* out, int cap);
DSE_CAPI int dse_physics3d_overlap_box(float min_x, float min_y, float min_z,
                                       float max_x, float max_y, float max_z,
                                       uint32_t* out, int cap);

// ============================================================
// Render 服务（L5，手写 dse_api_render.cpp）
// ============================================================
// world_to_screen：把 3D 世界坐标投影到屏幕像素，填充非空 out_sx/out_sy，返回 is_visible(0/1)。
DSE_CAPI int dse_render_world_to_screen(float wx, float wy, float wz,
                                        float* out_sx, float* out_sy);

// screen_to_world_ray：由屏幕像素 (sx,sy) 用主相机反投影出世界空间拾取射线。
// 填充非空 out_origin[3]（射线起点=相机位置）与 out_dir[3]（已归一化方向）。
// 成功（存在启用的主相机）返回 1，否则返回 0 且不修改输出。
DSE_CAPI int dse_render_screen_to_world_ray(float sx, float sy,
                                            float* out_origin, float* out_dir);

// MeshRenderer 材质/贴图加载（依赖 AssetManager）。
// set_material_from_dmat：从 .dmat 载入 MaterialInstance 并拷入 MeshRenderer，成功返回 1。
// set_texture：按 slot 名载入贴图并绑定到对应 handle，成功返回 1 并填充非空 out_*；slot 非法/加载失败返回 0。
DSE_CAPI int dse_mesh_renderer_set_material_from_dmat(uint32_t e, const char* dmat_path,
                                                      uint32_t material_index);
DSE_CAPI int dse_mesh_renderer_set_texture(uint32_t e, const char* slot, const char* path,
                                           uint32_t* out_handle, int* out_width, int* out_height);

// MeshRenderer 过程网格创作 + 材质创作（手写 dse_api_render.cpp）。
// add_procedural：emplace_or_replace 组件并写入过程顶点（xyz 扁平）与索引（越界索引丢弃）。
// set_material_params：浮点 NaN=保持当前值；receive_shadow/double_sided：-1=保持当前值。
// set_material_scalar：按名写标量并把材质数据源标记为 ComponentFallback；未知名仅标记。
// set_advanced_material：写高级材质参数并标记 ComponentFallback。
// set_uvs/set_normals/set_tangents：写入过程网格属性缓冲；返回属性数量与顶点数是否匹配(0/1)，
//   填充非空 out_attr_count（属性个数）与 out_vertex_count。
// set_emissive_authoring：写 emissive 并标记 ComponentFallback。
DSE_CAPI void dse_mesh_renderer_add_procedural(uint32_t e, float r, float g, float b, float a,
                                               const float* vertices, int vertex_float_count,
                                               const int* indices, int index_count);
DSE_CAPI void dse_mesh_renderer_set_material_params(uint32_t e, float metallic, float roughness,
                                                    float ao, float er, float eg, float eb,
                                                    float normal_strength,
                                                    int receive_shadow, int double_sided,
                                                    float cr, float cg, float cb, float ca);
DSE_CAPI void dse_mesh_renderer_set_depth_state(uint32_t e, int depth_test, int depth_write);
DSE_CAPI void dse_mesh_renderer_set_material_scalar(uint32_t e, const char* name, float value);
DSE_CAPI void dse_mesh_renderer_set_advanced_material(uint32_t e, float clear_coat,
                                                      float clear_coat_roughness, float anisotropy,
                                                      float pom_height_scale, float sss_strength,
                                                      float sss_r, float sss_g, float sss_b);
DSE_CAPI int  dse_mesh_renderer_set_uvs(uint32_t e, const float* uvs, int count,
                                        int* out_attr_count, int* out_vertex_count);
DSE_CAPI int  dse_mesh_renderer_set_normals(uint32_t e, const float* normals, int count,
                                            int* out_attr_count, int* out_vertex_count);
DSE_CAPI int  dse_mesh_renderer_set_tangents(uint32_t e, const float* tangents, int count,
                                             int* out_attr_count, int* out_vertex_count);
DSE_CAPI void dse_mesh_renderer_set_emissive_authoring(uint32_t e, float r, float g, float b);

// ---- Morph 简单权重组件（MorphComponent，区别于 MorphTargetComponent 的 dse_morph_*）。 ----
DSE_CAPI void  dse_morph_simple_add(uint32_t e);
DSE_CAPI void  dse_morph_simple_add_target(uint32_t e, const char* name, float weight);
DSE_CAPI void  dse_morph_simple_set_weight(uint32_t e, const char* name, float w);
DSE_CAPI void  dse_morph_simple_set_weight_index(uint32_t e, int idx, float w);
DSE_CAPI float dse_morph_simple_get_weight(uint32_t e, const char* name);
DSE_CAPI float dse_morph_simple_get_weight_index(uint32_t e, int idx);
DSE_CAPI void  dse_morph_simple_set_enabled(uint32_t e, int enabled);

// 供 L5 手写实现（dse_api_render.cpp）访问内部 AssetManager 指针
DSE_CAPI void* dse_get_asset_manager_ptr(void);

// ============================================================
// Gameplay3D（破碎 / 布料 / 流体，手写 dse_api_gameplay3d.cpp）
// ============================================================
// 纯 ECS 组件控制，操作全局 World。可选「保持当前值」字段以 NaN 哨兵表示；
// 固定默认值由调用方（Lua 薄包装）解析后传入。

// Fracture：source 0=Prefractured 1=RuntimeVoronoi。
DSE_CAPI void dse_fracture_add(uint32_t e, int source, uint32_t fragment_count,
                               float break_force, float health);
DSE_CAPI void dse_fracture_set_params(uint32_t e, float explosion_force, float fragment_lifetime,
                                      float fade_duration, float mass_scale);  // NaN=保持
DSE_CAPI void dse_fracture_apply_damage(uint32_t e, float damage, float ix, float iy, float iz);
DSE_CAPI void dse_fracture_trigger(uint32_t e, float ix, float iy, float iz);
DSE_CAPI int  dse_fracture_is_fractured(uint32_t e);

// Cloth。
DSE_CAPI void dse_cloth_add(uint32_t e, uint32_t solver_iterations, float stiffness,
                            float damping, float bend_stiffness);
DSE_CAPI void dse_cloth_set_wind(uint32_t e, float wx, float wy, float wz, float turbulence);  // turbulence NaN=保持
DSE_CAPI void dse_cloth_set_gravity(uint32_t e, float gx, float gy, float gz);
DSE_CAPI void dse_cloth_pin_vertices(uint32_t e, const uint32_t* vertices, int count);
DSE_CAPI void dse_cloth_add_sphere_collider(uint32_t e, uint32_t collider_entity, float radius);

// Fluid：shape 0=Point 1=Sphere 2=Box。
DSE_CAPI void dse_fluid_add_emitter(uint32_t e, int shape, float emission_rate,
                                    float particle_lifetime, float emit_speed);
DSE_CAPI void dse_fluid_set_physics(uint32_t e, float viscosity, float surface_tension,
                                    float rest_density, float gas_stiffness);  // NaN=保持
DSE_CAPI void dse_fluid_set_rendering(uint32_t e, float r, float g, float b, float a,
                                      float refraction, float fresnel, float specular);  // refraction/fresnel/specular NaN=保持
DSE_CAPI void dse_fluid_set_emit_direction(uint32_t e, float dx, float dy, float dz, float spread);  // spread NaN=保持
DSE_CAPI void dse_fluid_set_floor(uint32_t e, float floor_y, float restitution);  // NaN=保持
DSE_CAPI uint32_t dse_fluid_get_particle_count(uint32_t e);

// Ragdoll（仅设标志，激活由 RagdollSystem 处理）。auto_setup/active 用 int(0/1)。
DSE_CAPI void dse_ragdoll_add(uint32_t e, float total_mass, int auto_setup,
                              float joint_stiffness, float joint_damping);
DSE_CAPI void dse_ragdoll_activate(uint32_t e);
DSE_CAPI void dse_ragdoll_deactivate(uint32_t e);
DSE_CAPI int  dse_ragdoll_is_active(uint32_t e);
// ragdoll 逐字段 get/set 由 dse_api_ragdoll.gen.cpp 提供（codegen）
DSE_CAPI void dse_ragdoll_set_collision_layer(uint32_t e, int v);  // codegen 逐字段
DSE_CAPI void dse_ragdoll_set_collision_layer_mask(uint32_t e, uint32_t layer, uint32_t mask);

// SoftBody。gravity_scale NaN=保持。
DSE_CAPI void dse_softbody_add(uint32_t e, float stiffness, int iterations,
                               float damping, float volume_stiffness);
DSE_CAPI void dse_softbody_set_gravity(uint32_t e, int use_gravity, float gravity_scale);
DSE_CAPI void dse_softbody_pin_vertex(uint32_t e, int vertex_index);
DSE_CAPI uint32_t dse_softbody_get_particle_count(uint32_t e);

// Vehicle（raycast 车辆）。set_input 内部 clamp 到合法范围。
DSE_CAPI void dse_vehicle_add(uint32_t e, float max_engine_force, float max_brake_force,
                              float max_steer_angle);
DSE_CAPI void dse_vehicle_add_wheel(uint32_t e, float px, float py, float pz, float radius,
                                    int is_drive, int is_steer, float susp_stiffness,
                                    float susp_damping);
DSE_CAPI void dse_vehicle_set_input(uint32_t e, float throttle, float brake, float steering);
DSE_CAPI float dse_vehicle_get_speed(uint32_t e);
DSE_CAPI uint32_t dse_vehicle_get_wheel_count(uint32_t e);

// Rope。get_positions：填充 out_xyz（最多 max_points 点×3 float），返回点总数；
// out_xyz=null 时仅返回总数供预分配。gravity_scale NaN=保持。
DSE_CAPI void dse_rope_add(uint32_t e, int segment_count, float segment_length,
                           float damping, int iterations);
DSE_CAPI void dse_rope_set_anchors(uint32_t e, uint32_t anchor_a, uint32_t anchor_b,
                                   float oax, float oay, float oaz,
                                   float obx, float oby, float obz);
DSE_CAPI int  dse_rope_get_positions(uint32_t e, float* out_xyz, int max_points);
DSE_CAPI void dse_rope_set_gravity(uint32_t e, int use_gravity, float gravity_scale);

// Buoyancy。
DSE_CAPI void dse_buoyancy_add(uint32_t e, float water_level, float buoyancy_force,
                               float water_drag, float angular_drag, float submerge_depth);
DSE_CAPI void dse_buoyancy_add_sample_point(uint32_t e, float ox, float oy, float oz,
                                            float force_scale);
DSE_CAPI void dse_buoyancy_set_water_level(uint32_t e, float water_level);  // codegen 逐字段
DSE_CAPI float dse_buoyancy_get_submerge_ratio(uint32_t e);
DSE_CAPI void dse_buoyancy_set_use_fluid(uint32_t e, int use_fluid);

// ---- Batch 3 环境子系统（无物理依赖）。浮点 NaN=保持当前值。 ----
// Weather。type: 0=None,1=Rain,2=Snow；set 中 type<0=保持。spawn 中 max_particles<0=保持。
DSE_CAPI void dse_weather_add(uint32_t e, int type, float intensity);
DSE_CAPI void dse_weather_set(uint32_t e, int type, float intensity,
                              float wind_x, float wind_z);
DSE_CAPI void dse_weather_set_spawn(uint32_t e, float radius, float height,
                                    int max_particles);

// SnowCover。snow_cover_get 填充 out_*（可为 null），返回 1=存在/0=缺失。
// set_texture：path=null 仅改 tiling。
DSE_CAPI void dse_snow_cover_add(uint32_t e);
DSE_CAPI void dse_snow_cover_set(uint32_t e, float target_coverage,
                                 float accumulation_rate, float melt_rate);
DSE_CAPI void dse_snow_set_appearance(uint32_t e, float albedo_r, float albedo_g,
                                      float albedo_b, float roughness, float metallic,
                                      float threshold, float sharpness);
DSE_CAPI int  dse_snow_cover_get(uint32_t e, float* out_coverage,
                                 float* out_target, int* out_enabled);
DSE_CAPI void dse_snow_cover_set_enabled(uint32_t e, int enabled);
DSE_CAPI void dse_snow_set_texture(uint32_t e, const char* path, float tiling);
DSE_CAPI void dse_snow_set_displacement(uint32_t e, float displacement_height,
                                        float deformation_strength);
DSE_CAPI void dse_snow_cover_remove(uint32_t e);

// Atmosphere（物理天空参数）。
DSE_CAPI void dse_atmosphere_add(uint32_t e);
DSE_CAPI void dse_atmosphere_set_params(uint32_t e, float planet_radius,
                                        float atmosphere_height, float sun_disk_angle);
DSE_CAPI void dse_atmosphere_set_rayleigh(uint32_t e, float coeff_r, float coeff_g,
                                          float coeff_b, float scale_height);
DSE_CAPI void dse_atmosphere_set_mie(uint32_t e, float coeff, float scale_height, float g);
DSE_CAPI void dse_atmosphere_set_sun_intensity(uint32_t e, float x, float y, float z);  // codegen 逐字段 vec3

// DayNightCycle。set_location 中 day_of_year<=0=保持。get_sun_direction 填充 out_xyz(3)。
DSE_CAPI void dse_day_night_add(uint32_t e, float time_of_day, int auto_advance,
                                float time_speed);
DSE_CAPI void dse_day_night_set_time(uint32_t e, float time_of_day);
DSE_CAPI float dse_day_night_get_time(uint32_t e);
DSE_CAPI void dse_day_night_set_speed(uint32_t e, float speed);
DSE_CAPI void dse_day_night_set_auto_advance(uint32_t e, int enabled);  // codegen 逐字段
DSE_CAPI void dse_day_night_set_location(uint32_t e, float latitude, float longitude,
                                         int day_of_year);
DSE_CAPI float dse_day_night_get_sun_elevation(uint32_t e);
DSE_CAPI void dse_day_night_get_sun_direction(uint32_t e, float* out_xyz);

// VolumetricCloud。
DSE_CAPI void dse_volumetric_cloud_add(uint32_t e);
DSE_CAPI void dse_cloud_set_layer(uint32_t e, float bottom, float top,
                                  float coverage, float density);
DSE_CAPI void dse_cloud_set_wind(uint32_t e, float dir_x, float dir_y, float speed);

// ============================================================
// 动画子系统（L4/L5，纯 ECS）。浮点 NaN=保持当前值。
// ============================================================

// ---- 2D 帧动画（AnimatorComponent）。add_state: loop 为 0/1；frame_handles 可为 null。
// pop_event 将事件名写入 out（null 结尾，按 cap 截断），返回 1=弹出/0=无。 ----
DSE_CAPI void dse_anim2d_add(uint32_t e);
DSE_CAPI void dse_anim2d_add_state(uint32_t e, const char* name, float fps, int loop,
                                   const uint32_t* frame_handles, int handle_count);
DSE_CAPI void dse_anim2d_add_event(uint32_t e, const char* state_name,
                                   float normalized_time, const char* event_name);
DSE_CAPI void dse_anim2d_play(uint32_t e, const char* state_name);
DSE_CAPI void dse_anim2d_play_segment(uint32_t e, int start_frame, int end_frame, int loop);
DSE_CAPI int  dse_anim2d_pop_event(uint32_t e, char* out, int cap);

// ---- 3D 骨骼动画 / 状态机（Animator3DComponent）。
// set_state: state_name=null 不改状态，speed=NaN 保持，loop<0 保持。
// get_state: 填充 out_*（均可为 null），返回 1=存在/0=缺失。
// add_transition: 条件以并行扁平数组传入（names/modes/thresholds/ints，长度 cond_count）。 ----
DSE_CAPI void dse_anim3d_add(uint32_t e, const char* danim_path, const char* dskel_path);
DSE_CAPI void dse_anim3d_set_state(uint32_t e, const char* state_name, float speed, int loop);
DSE_CAPI int  dse_anim3d_get_state(uint32_t e, char* out_state, int state_cap,
                                   float* out_norm, float* out_time, float* out_speed,
                                   int* out_loop, int* out_transitioning,
                                   int* out_bone_count, int* out_has_skel);
DSE_CAPI void dse_anim3d_init_fsm(uint32_t e);
DSE_CAPI void dse_anim3d_add_fsm_state(uint32_t e, const char* state_name,
                                       const char* danim_path, int loop, float speed);
DSE_CAPI void dse_anim3d_add_transition(uint32_t e, const char* from_state,
                                        const char* to_state, float transition_duration,
                                        int has_exit_time, float exit_time,
                                        int cond_count,
                                        const char* const* cond_names,
                                        const int* cond_modes,
                                        const float* cond_thresholds,
                                        const int* cond_ints);
// Lua 绑定薄包装：init_fsm 带可选 default_state；无条件 transition
DSE_CAPI void dse_compat_anim3d_init_fsm(uint32_t e, const char* default_state);
DSE_CAPI void dse_compat_anim3d_add_transition(uint32_t e, const char* from_state,
                                               const char* to_state, float transition_duration,
                                               int has_exit_time, float exit_time);
// ---- S1.9 3D compat 薄包装（定义见 dse_api_gameplay3d.cpp） ----
DSE_CAPI void dse_compat_weather_add(uint32_t e, const char* type, float intensity);
DSE_CAPI void dse_compat_set_directional_light_3d(uint32_t e, int enabled,
        float dx, float dy, float dz, float r, float g, float b,
        float intensity, float ambient, float shadow_strength);
DSE_CAPI void dse_compat_set_point_light_3d(uint32_t e, float r, float g, float b,
        float intensity, float radius);
DSE_CAPI void dse_compat_set_spot_light_3d(uint32_t e, float dx, float dy, float dz,
        float r, float g, float b, float intensity, float radius, float inner, float outer);
DSE_CAPI void dse_compat_world_to_screen(float wx, float wy, float wz,
        float* out_sx, float* out_sy, int* out_visible);
DSE_CAPI void dse_anim3d_set_param_float(uint32_t e, const char* param_name, float value);
DSE_CAPI void dse_anim3d_set_param_trigger(uint32_t e, const char* param_name);
DSE_CAPI void dse_anim3d_set_lock_root_motion(uint32_t e, int lock);
DSE_CAPI void dse_anim3d_add_event(uint32_t e, const char* event_name, float trigger_time);
DSE_CAPI int  dse_anim3d_pop_event(uint32_t e, char* out, int cap);
DSE_CAPI void dse_anim3d_set_extract_root_motion(uint32_t e, int enabled);
DSE_CAPI int  dse_anim3d_get_root_motion_delta(uint32_t e, float* out_xyz);

// ---- 动画层 / 混合树（AnimLayerComponent）。add 返回层索引（-1=无组件）。
// blend_tree_1d: paths/thresholds/speeds 并行数组，长度 count。 ----
DSE_CAPI void dse_animlayer_add_component(uint32_t e);
DSE_CAPI int  dse_animlayer_add(uint32_t e, const char* name, float weight, int blend_mode);
DSE_CAPI void dse_animlayer_set_clip(uint32_t e, int idx, const char* danim_path,
                                     float speed, int loop);
DSE_CAPI void dse_animlayer_set_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_animlayer_set_bone_mask(uint32_t e, int idx,
                                          const char* const* bones, int count);
DSE_CAPI void dse_animlayer_set_blend_tree_1d(uint32_t e, int idx,
                                              const char* const* paths,
                                              const float* thresholds,
                                              const float* speeds, int count);
DSE_CAPI void dse_animlayer_set_blend_param(uint32_t e, int idx, float val);
DSE_CAPI void dse_animlayer_set_enabled(uint32_t e, int enabled);

// ---- IK（IKChain3DComponent）。add_chain 返回链索引（-1=无组件）。
// set_target_entity: target=UINT32_MAX 清除目标实体。 ----
DSE_CAPI void dse_ik_add_component(uint32_t e);
DSE_CAPI int  dse_ik_add_chain(uint32_t e, const char* name, int type,
                               const char* root_bone, const char* tip_bone, float weight);
DSE_CAPI void dse_ik_set_target(uint32_t e, int idx, float x, float y, float z);
DSE_CAPI void dse_ik_set_target_entity(uint32_t e, int idx, uint32_t target);

// Outfit/clothing: cross-entity skeleton reference for MeshRendererComponent.
// skeleton_entity = UINT32_MAX clears the reference (use own Animator3D).
// Getter returns UINT32_MAX when unset or the component is missing.
DSE_CAPI void     dse_mesh_renderer_set_skeleton(uint32_t e, uint32_t skeleton_entity);
DSE_CAPI uint32_t dse_mesh_renderer_get_skeleton(uint32_t e);
DSE_CAPI void dse_ik_set_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_ik_set_pole_vector(uint32_t e, int idx, float x, float y, float z);
DSE_CAPI void dse_ik_set_iterations(uint32_t e, int idx, int iters);
DSE_CAPI void dse_ik_set_enabled(uint32_t e, int enabled);

// ---- FootIK（FootIK3DComponent）。脚部贴地，依赖物理 Raycast 检测地面。
// add_foot 返回脚索引（-1=无组件）；浮点参数为 NaN 时保持默认值。 ----
DSE_CAPI void dse_foot_ik_add_component(uint32_t e);
DSE_CAPI int  dse_foot_ik_add_foot(uint32_t e, const char* name, const char* foot_bone,
                                   const char* hip_bone, float foot_height,
                                   float max_ground_distance, float blend_speed, float weight);
DSE_CAPI void dse_foot_ik_set_foot_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_foot_ik_set_foot_height(uint32_t e, int idx, float h);
DSE_CAPI void dse_foot_ik_set_pelvis(uint32_t e, float pelvis_weight, float max_pelvis_offset);
DSE_CAPI void dse_foot_ik_set_enabled(uint32_t e, int enabled);

// ---- 骨骼挂点（BoneAttachmentComponent）。set_offset: 缩放 sx/sy/sz 为 NaN 时取 1。
// get_world_pos: 由目标实体动画姿态计算，out_xyz(3) 始终写入，返回 1=成功/0=失败。 ----
DSE_CAPI void dse_bone_attach_add(uint32_t e, uint32_t target, const char* bone_name);
DSE_CAPI void dse_bone_attach_set_offset(uint32_t e, float px, float py, float pz,
                                         float qx, float qy, float qz, float qw,
                                         float sx, float sy, float sz);
DSE_CAPI void dse_bone_attach_set_bone(uint32_t e, const char* bone_name);
DSE_CAPI void dse_bone_attach_set_target(uint32_t e, uint32_t target);
DSE_CAPI int  dse_bone_attach_get_world_pos(uint32_t target, const char* bone_name,
                                            float* out_xyz);
DSE_CAPI void dse_bone_attach_remove(uint32_t e);

// ---- Morph Target / Blend Shape（MorphTargetComponent）。
// add_target: deltas 扁平布局，每顶点 6 float（dpx,dpy,dpz,dnx,dny,dnz）。 ----
DSE_CAPI void  dse_morph_add_component(uint32_t e);
DSE_CAPI void  dse_morph_add_target(uint32_t e, const char* name,
                                    const float* deltas, int float_count);
DSE_CAPI void  dse_morph_set_weight(uint32_t e, const char* name, float w);
DSE_CAPI void  dse_morph_set_weight_index(uint32_t e, int idx, float w);
DSE_CAPI float dse_morph_get_weight(uint32_t e, const char* name);
DSE_CAPI int   dse_morph_get_target_count(uint32_t e);

// ---- Jiggle Bone / Spring Bone（乳摇，JiggleBoneComponent）。
// 标量字段 get/set 见 dse_api.gen.h；以下为运行期列表操作（手写）。
// 浮点参数 NaN = 保持当前值；add_* 返回索引（失败 -1）。 ----
DSE_CAPI void dse_jiggle_add_component(uint32_t e);
DSE_CAPI void dse_jiggle_remove_component(uint32_t e);
DSE_CAPI void dse_jiggle_clear_bones(uint32_t e);
DSE_CAPI int  dse_jiggle_get_bone_count(uint32_t e);
DSE_CAPI int  dse_jiggle_add_bone(uint32_t e, const char* bone_name,
                                  float stiffness, float damping,
                                  float gravity, float bone_length);
DSE_CAPI void dse_jiggle_set_bone_params(uint32_t e, int index,
                                         float stiffness, float damping,
                                         float gravity, float bone_length);
DSE_CAPI void dse_jiggle_set_bone_gravity_dir(uint32_t e, int index,
                                              float x, float y, float z);
DSE_CAPI void dse_jiggle_clear_colliders(uint32_t e);
DSE_CAPI int  dse_jiggle_get_collider_count(uint32_t e);
DSE_CAPI int  dse_jiggle_add_collider(uint32_t e, const char* bone_name,
                                      float cx, float cy, float cz, float radius);

// ============================================================

// Particles 3D
// ============================================================

// 浮点参数 NaN=保持当前值；mode<0=保持当前值；texture_path=NULL 保持当前值。

DSE_CAPI void  dse_particle_system_3d_add(uint32_t e, int max_particles, float emission_rate);
DSE_CAPI void  dse_particle_system_3d_set_params(uint32_t e,
                                                 float life_min, float life_max,
                                                 float size_min, float size_max,
                                                 float speed_min, float speed_max,
                                                 float r, float g, float b, float a,
                                                 float gx, float gy, float gz,
                                                 const char* texture_path);
// out_life/out_size/out_speed: float[2]，out_gravity: float[3]，out_color: float[4]。
// 组件存在返回 1，否则 0。out_tex 写入纹理路径（cap 截断，null 结尾）。
DSE_CAPI int   dse_particle_system_3d_get_state(uint32_t e, int* out_active, int* out_max_particles,
                                                float* out_emission_rate,
                                                float* out_life, float* out_size, float* out_speed,
                                                float* out_gravity, float* out_color,
                                                char* out_tex, int tex_cap,
                                                int* out_enabled, int* out_initialized,
                                                uint32_t* out_texture_handle);
DSE_CAPI void  dse_particle_emitter_add(uint32_t e, uint32_t texture_handle, int max_particles, float emit_rate);
DSE_CAPI void  dse_particle_set_density(uint32_t e, float emit_rate_scale);
DSE_CAPI void  dse_particle_burst(uint32_t e, int count);
DSE_CAPI void  dse_particle_set_random(uint32_t e,
                                       float vmin_x, float vmin_y, float vmin_z,
                                       float vmax_x, float vmax_y, float vmax_z,
                                       float life_min, float life_max,
                                       float size_min, float size_max);
DSE_CAPI void  dse_particle_set_size_curve(uint32_t e, int enabled, float start_value, float end_value);
DSE_CAPI void  dse_particle_set_alpha_curve(uint32_t e, int enabled, float start_value, float end_value);
DSE_CAPI void  dse_particle_set_speed_curve(uint32_t e, int enabled, float start_value, float end_value);
DSE_CAPI void  dse_particle_set_gravity(uint32_t e, float gx, float gy, float gz);
DSE_CAPI void  dse_particle_set_collision(uint32_t e, int enabled, int mode, float bounce,
                                          float friction, float life_loss, float ground_y);
DSE_CAPI void  dse_particle_set_color_curve(uint32_t e, int enabled,
                                            float end_r, float end_g, float end_b, float end_a);
DSE_CAPI void  dse_particle_set_rotation(uint32_t e, float rotation_min, float rotation_max,
                                         float angular_velocity_min, float angular_velocity_max);
DSE_CAPI void  dse_gameplay_tuning_add(uint32_t e);
DSE_CAPI void  dse_gameplay_tuning_set(uint32_t e, float leaf_min_distance,
                                       float leaf_move_left, float leaf_move_right,
                                       float jump_speed_scale, float jump_speed_max,
                                       float camera_follow_damping);

// ============================================================
// Rendering Light（运行时动态操作）
// ============================================================

DSE_CAPI void  dse_rendering_add_skybox(uint32_t e, const char* cubemap_path);
DSE_CAPI void  dse_rendering_add_gi_probe(uint32_t e);
DSE_CAPI void  dse_rendering_set_gi_probe(uint32_t e, float gi_intensity, float ox, float oy, float oz,
                                          float ex, float ey, float ez,
                                          int res_x, int res_y, int res_z);
DSE_CAPI void  dse_rendering_set_gi_probe_enabled(uint32_t e, int enabled);
DSE_CAPI int   dse_rendering_get_gi_probe(uint32_t e, float* out_gi_intensity,
                                          float* out_origin, float* out_extent, int* out_resolution);
DSE_CAPI void  dse_rendering_add_light_probe(uint32_t e);
DSE_CAPI void  dse_rendering_set_light_probe(uint32_t e, float influence_radius);
DSE_CAPI void  dse_rendering_set_light_probe_enabled(uint32_t e, int enabled);
DSE_CAPI void  dse_rendering_add_reflection_probe(uint32_t e);
DSE_CAPI void  dse_rendering_set_reflection_probe(uint32_t e, float influence_radius, int resolution);
DSE_CAPI void  dse_rendering_set_reflection_probe_enabled(uint32_t e, int enabled);

// 灯光/探针补充（浮点 NaN=保持当前值；int -1=保持当前值）。has/get 系列组件缺失时返回 0。
DSE_CAPI int   dse_dir_light_has(uint32_t e);
DSE_CAPI int   dse_point_light_has(uint32_t e);
DSE_CAPI int   dse_spot_light_has(uint32_t e);
DSE_CAPI int   dse_sky_light_has(uint32_t e);
DSE_CAPI int   dse_dir_light_get_shadow_params(uint32_t e, int* out_cast_shadow, float* out_strength,
                                               float* out_c0, float* out_c1, float* out_c2,
                                               float* out_lambda);
DSE_CAPI void  dse_rendering_set_gi_probe_bias(uint32_t e, float normal_bias, float hysteresis);
DSE_CAPI int   dse_rendering_get_gi_probe_ex(uint32_t e, int* out_enabled, float* out_normal_bias);
DSE_CAPI void  dse_rendering_set_light_probe_ex(uint32_t e, float influence_radius, int needs_rebake);
DSE_CAPI void  dse_rendering_set_reflection_probe_ex(uint32_t e, float influence_radius,
                                                     float box_x, float box_y, float box_z,
                                                     int resolution);

// ============================================================
// Rendering Camera 扩展
// ============================================================

DSE_CAPI void  dse_camera_add(uint32_t e, float ortho_size, int priority);
DSE_CAPI void  dse_camera_set_priority(uint32_t e, int priority);
DSE_CAPI void  dse_camera_set_enabled(uint32_t e, int enabled);
DSE_CAPI void  dse_camera_set_follow(uint32_t e, uint32_t target, float damping,
                                     float dead_zone_x, float dead_zone_y,
                                     float offset_x, float offset_y);
DSE_CAPI void  dse_free_camera_add(uint32_t e, float move_speed, float mouse_sensitivity);
DSE_CAPI void  dse_sprite_add(uint32_t e, float r, float g, float b, float a,
                              int order_in_layer, uint32_t texture_handle);
DSE_CAPI void  dse_sprite_set_uv_scroll(uint32_t e, float sx, float sy);
DSE_CAPI void  dse_sprite_set_uv_offset(uint32_t e, float ox, float oy);

// ============================================================
// Rendering Mesh 扩展
// ============================================================

DSE_CAPI void  dse_mesh_set_material(uint32_t e, const char* material_path);
DSE_CAPI void  dse_mesh_set_depth_state(uint32_t e, int depth_test, int depth_write);
DSE_CAPI void  dse_mesh_set_material_scalar(uint32_t e, const char* param_name, float value);
DSE_CAPI void  dse_mesh_set_texture_handle(uint32_t e, const char* slot, uint32_t texture_handle);
DSE_CAPI void  dse_mesh_set_emissive(uint32_t e, float r, float g, float b);

// ============================================================
// Rendering FX（Steering / LOD / Hair / Pick）
// ============================================================

DSE_CAPI void  dse_steering_add(uint32_t e, float max_velocity, float max_force, float mass);
// behavior: 0=seek 1=flee 2=arrive；返回 1=成功。
DSE_CAPI int   dse_steering_set_target(uint32_t e, int behavior, float x, float y, float z);
// out_flags[4]=enabled/seek/flee/arrive；out_velocity[3]；out_params[4]=max_velocity/max_force/mass/arrive_deceleration_radius；
// out_targets[9]=seek/flee/arrive 各 3 分量。返回 1=组件存在。
DSE_CAPI int   dse_steering_get_state(uint32_t e, int* out_flags, float* out_velocity,
                                      float* out_params, float* out_targets);

DSE_CAPI void  dse_lod_add_level(uint32_t e, const char* mesh_path, float screen_size_threshold);
DSE_CAPI void  dse_lod_set_scale(uint32_t e, float scale);
DSE_CAPI void  dse_lod_set_min_screen_size(uint32_t e, float min_size);
DSE_CAPI void  dse_lod_set_enabled(uint32_t e, int enabled);

// num_follow_per_guide<0=保持默认。浮点 NaN=保持当前值。
// dse_hair_set_enabled 见 dse_api.gen.h（codegen 逐字段 setter）。
DSE_CAPI void  dse_hair_add(uint32_t e, const char* asset_path, int num_follow_per_guide);
DSE_CAPI void  dse_hair_set_physics(uint32_t e, float damping, float stiffness_local,
                                    float stiffness_global, float gravity);
DSE_CAPI void  dse_hair_set_render(uint32_t e,
                                   float root_r, float root_g, float root_b, float root_a,
                                   float tip_r, float tip_g, float tip_b, float tip_a,
                                   float fiber_radius, float opacity);
DSE_CAPI void  dse_hair_set_wind_full(uint32_t e, float wx, float wy, float wz, float turbulence);
DSE_CAPI void  dse_hair_set_lod(uint32_t e, float lod0_distance, float lod1_distance,
                                float lod2_distance, float cull_distance);

// ============================================================
// Rendering Post 扩展（Decal / PostProcess state query）
// ============================================================

DSE_CAPI void  dse_decal_add(uint32_t e, uint32_t albedo_texture);
DSE_CAPI void  dse_decal_set(uint32_t e, float r, float g, float b, float a, float angle_fade);
DSE_CAPI int   dse_post_process_get_state(uint32_t e, int* out_enabled, int* out_bloom, int* out_ssao,
                                           int* out_ssr, int* out_fxaa, int* out_dof);
DSE_CAPI int   dse_post_process_set_color(uint32_t e, int enabled, float exposure, float gamma);
DSE_CAPI int   dse_post_process_get_color_state(uint32_t e, int* out_enabled, int* out_bloom_enabled,
                                                float* out_bloom_threshold, float* out_bloom_intensity,
                                                int* out_color_enabled, float* out_exposure, float* out_gamma,
                                                int* out_ssao_enabled, float* out_ssao_radius, float* out_ssao_bias,
                                                int* out_fxaa_enabled, int* out_vignette_enabled,
                                                float* out_vignette_intensity, float* out_vignette_radius,
                                                float* out_vignette_softness, int* out_film_grain_enabled,
                                                float* out_film_grain_intensity, float* out_film_grain_time_scale);

// ============================================================
// Animation 扩展
// ============================================================

DSE_CAPI void  dse_anim3d_set_blend_tree_1d(uint32_t e, const char* const* clips, const float* thresholds,
                                             const float* speeds, int count);
DSE_CAPI void  dse_anim3d_set_blend_param(uint32_t e, float value);
DSE_CAPI float dse_anim3d_get_blend_param(uint32_t e);
DSE_CAPI void  dse_anim3d_set_layer_weight(uint32_t e, int layer, float weight);
DSE_CAPI float dse_anim3d_get_layer_weight(uint32_t e, int layer);
DSE_CAPI void  dse_anim3d_set_layer_mask(uint32_t e, int layer, const char* const* bones, int count);

// ============================================================
// Gameplay3D 扩展
// ============================================================

DSE_CAPI int   dse_character_check_ground(uint32_t e, float* out_normal);

// ============================================================
// Open World（WorldPartition / HLOD / VirtualTexture / Clipmap / SDF / AI LoD / GpuParticle / WorldStatePersistence / Procedural）
// ============================================================

DSE_CAPI int   dse_wp_get_loaded_count(void);
DSE_CAPI int   dse_wp_force_load(int cx, int cz);
DSE_CAPI int   dse_wp_force_unload(int cx, int cz);
DSE_CAPI void  dse_wp_world_to_cell(float x, float y, float z, float cell_size, int* out_cx, int* out_cz);
DSE_CAPI void  dse_wp_cell_to_world(int cx, int cz, float cell_size, float* out_x, float* out_y, float* out_z);

DSE_CAPI int   dse_hlod_get_cluster_count(void);
DSE_CAPI int   dse_hlod_get_active_proxy_count(void);

DSE_CAPI float dse_vt_get_cache_hit_rate(void);
DSE_CAPI int   dse_vt_get_page_table_size(void);
DSE_CAPI int   dse_vt_get_physical_atlas_size(void);
DSE_CAPI int   dse_vt_get_occupied_pages(void);

DSE_CAPI int   dse_clipmap_get_level_count(void);
DSE_CAPI int   dse_clipmap_sample_height(float x, float z, float* out_y);
DSE_CAPI void  dse_clipmap_get_config(float* out_cell_size, int* out_levels);

DSE_CAPI float dse_sdf_query_distance(float x, float y, float z);
DSE_CAPI int   dse_sdf_get_cascade_count(void);
DSE_CAPI int   dse_sdf_rebuild(void);

DSE_CAPI void  dse_ai_lod_register(uint32_t e, float importance);
DSE_CAPI void  dse_ai_lod_unregister(uint32_t e);
DSE_CAPI int   dse_ai_lod_should_tick(uint32_t e);
DSE_CAPI int   dse_ai_lod_get_level(uint32_t e);
DSE_CAPI void  dse_ai_lod_set_force_active(uint32_t e, int force);
DSE_CAPI int   dse_ai_lod_get_registered_count(void);
DSE_CAPI void  dse_ai_lod_get_config(float* out_near_dist, float* out_far_dist, int* out_max_level);

DSE_CAPI void  dse_gpu_particle_set_enabled(uint32_t e, int enabled);
DSE_CAPI void  dse_gpu_particle_set_emission_rate(uint32_t e, float rate);
DSE_CAPI void  dse_gpu_particle_set_gravity(uint32_t e, float gx, float gy, float gz);
DSE_CAPI void  dse_gpu_particle_set_wind(uint32_t e, float wx, float wy, float wz);
DSE_CAPI void  dse_gpu_particle_set_color(uint32_t e, float r1, float g1, float b1, float a1,
                                         float r2, float g2, float b2, float a2);

DSE_CAPI int   dse_wsp_save_all(void);
DSE_CAPI int   dse_wsp_save_cell(int cx, int cz);
DSE_CAPI int   dse_wsp_load_cell(int cx, int cz);
DSE_CAPI int   dse_wsp_reset_cell(int cx, int cz);
DSE_CAPI int   dse_wsp_get_dirty_count(void);
DSE_CAPI int   dse_wsp_get_total_modifications(void);
DSE_CAPI void  dse_wsp_record_destruction(int cx, int cz, uint64_t entity_id);

DSE_CAPI float dse_procedural_perlin2d(float x, float y, uint32_t seed);
DSE_CAPI float dse_procedural_simplex2d(float x, float y, uint32_t seed);
DSE_CAPI float dse_procedural_worley2d(float x, float y, uint32_t seed);
DSE_CAPI float dse_procedural_fbm2d(float x, float y, int octaves, float frequency,
                                    float lacunarity, float persistence, uint32_t seed);
DSE_CAPI void  dse_procedural_random_seed(uint64_t seed);
DSE_CAPI float dse_procedural_random_float(float min_val, float max_val);

// ============================================================

// PostProcess — 组件创建 / LUT 加载
// ============================================================

DSE_CAPI void  dse_post_process_add(uint32_t e);
DSE_CAPI void  dse_post_process_set_color_lut(uint32_t e, const char* path, float intensity);

// ============================================================

// Decal — 组件创建 / 扩展字段设置
// ============================================================

DSE_CAPI void  dse_decal_add_simple(uint32_t e);
DSE_CAPI void  dse_decal_set_full(uint32_t e, int enabled, int has_texture, uint32_t texture,
                                   float r, float g, float b, float a, float angle_fade);

// ============================================================



#ifdef __cplusplus
}
#endif

#endif // DSE_API_RENDER_H
