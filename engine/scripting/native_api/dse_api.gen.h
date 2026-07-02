/**
 * @file dse_api.gen.h
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * 迁移路径说明：
 *   当前（S1）：手写的 dse_api.h / dse_api.cpp 是权威实现，本文件为 staging。
 *   未来（S2+）：手写文件被废弃后，本文件升级为权威头文件。
 *   在过渡期内，不要同时 #include "dse_api.h" 和 "dse_api.gen.h"。
 */

#pragma once

// 若 dse_api.h 已被包含（手写版本），直接复用其声明，不产生重复符号。
#ifndef DSE_API_H
#include <stdint.h>

#ifdef _WIN32
#  define DSE_CAPI_GEN __declspec(dllexport)
#else
#  define DSE_CAPI_GEN __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ---- TransformComponent ---- */
DSE_CAPI_GEN void dse_transform_get_position(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_transform_set_position(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_transform_set_rotation(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_transform_set_scale(uint32_t e, float x, float y, float z);

/* ---- Camera3DComponent ---- */
DSE_CAPI_GEN float dse_camera3d_get_fov(uint32_t e);
DSE_CAPI_GEN void  dse_camera3d_set_fov(uint32_t e, float v);
DSE_CAPI_GEN float dse_camera3d_get_near_clip(uint32_t e);
DSE_CAPI_GEN void  dse_camera3d_set_near_clip(uint32_t e, float v);
DSE_CAPI_GEN float dse_camera3d_get_far_clip(uint32_t e);
DSE_CAPI_GEN void  dse_camera3d_set_far_clip(uint32_t e, float v);
DSE_CAPI_GEN int  dse_camera3d_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_camera3d_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_camera3d_get_priority(uint32_t e);
DSE_CAPI_GEN void dse_camera3d_set_priority(uint32_t e, int v);

/* ---- MeshRendererComponent ---- */
DSE_CAPI_GEN void dse_mesh_renderer_get_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_mesh_renderer_set_color(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN int  dse_mesh_renderer_get_visible(uint32_t e);
DSE_CAPI_GEN void dse_mesh_renderer_set_visible(uint32_t e, int v);
DSE_CAPI_GEN float dse_mesh_renderer_get_metallic(uint32_t e);
DSE_CAPI_GEN void  dse_mesh_renderer_set_metallic(uint32_t e, float v);
DSE_CAPI_GEN float dse_mesh_renderer_get_roughness(uint32_t e);
DSE_CAPI_GEN void  dse_mesh_renderer_set_roughness(uint32_t e, float v);
DSE_CAPI_GEN void dse_mesh_renderer_get_emissive(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_mesh_renderer_set_emissive(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_mesh_renderer_get_receive_shadow(uint32_t e);
DSE_CAPI_GEN void dse_mesh_renderer_set_receive_shadow(uint32_t e, int v);
DSE_CAPI_GEN int  dse_mesh_renderer_get_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN void dse_mesh_renderer_set_shader_variant(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_mesh_renderer_get_shader_variant(uint32_t e, char* buf, int buf_size);

/* ---- DirectionalLight3DComponent ---- */
DSE_CAPI_GEN void dse_dir_light_get_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_dir_light_set_direction(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_dir_light_get_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_dir_light_set_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_dir_light_get_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_dir_light_set_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_dir_light_get_ambient_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_dir_light_set_ambient_intensity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_dir_light_get_cast_shadow(uint32_t e);
DSE_CAPI_GEN void dse_dir_light_set_cast_shadow(uint32_t e, int v);
DSE_CAPI_GEN float dse_dir_light_get_shadow_strength(uint32_t e);
DSE_CAPI_GEN void  dse_dir_light_set_shadow_strength(uint32_t e, float v);
DSE_CAPI_GEN int  dse_dir_light_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_dir_light_set_enabled(uint32_t e, int v);

/* ---- PointLightComponent ---- */
DSE_CAPI_GEN void dse_point_light_get_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_point_light_set_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_point_light_get_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_point_light_set_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_point_light_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_point_light_set_radius(uint32_t e, float v);
DSE_CAPI_GEN int  dse_point_light_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_point_light_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_point_light_get_cast_shadow(uint32_t e);
DSE_CAPI_GEN void dse_point_light_set_cast_shadow(uint32_t e, int v);

/* ---- SpotLightComponent ---- */
DSE_CAPI_GEN void dse_spot_light_get_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_spot_light_set_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_spot_light_get_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_spot_light_set_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_spot_light_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_spot_light_set_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_spot_light_get_inner_cone_angle(uint32_t e);
DSE_CAPI_GEN void  dse_spot_light_set_inner_cone_angle(uint32_t e, float v);
DSE_CAPI_GEN float dse_spot_light_get_outer_cone_angle(uint32_t e);
DSE_CAPI_GEN void  dse_spot_light_set_outer_cone_angle(uint32_t e, float v);
DSE_CAPI_GEN void dse_spot_light_get_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_spot_light_set_direction(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_spot_light_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_spot_light_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_spot_light_get_cast_shadow(uint32_t e);
DSE_CAPI_GEN void dse_spot_light_set_cast_shadow(uint32_t e, int v);

/* ---- SkyLightComponent ---- */
DSE_CAPI_GEN void dse_sky_light_get_up_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_sky_light_set_up_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_sky_light_get_down_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_sky_light_set_down_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_sky_light_get_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_sky_light_set_intensity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_sky_light_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_sky_light_set_enabled(uint32_t e, int v);

/* ---- TreeComponent ---- */
DSE_CAPI_GEN int  dse_tree_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_tree_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_tree_get_density(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_density(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_spawn_radius(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_spawn_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_chunk_size(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_chunk_size(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_min_scale(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_min_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_max_scale(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_max_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_lod1_distance(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_lod1_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_cull_distance(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_cull_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_wind_strength(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_wind_strength(uint32_t e, float v);
DSE_CAPI_GEN float dse_tree_get_wind_speed(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_wind_speed(uint32_t e, float v);
DSE_CAPI_GEN int  dse_tree_get_cast_shadow(uint32_t e);
DSE_CAPI_GEN void dse_tree_set_cast_shadow(uint32_t e, int v);
DSE_CAPI_GEN float dse_tree_get_shadow_distance(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_shadow_distance(uint32_t e, float v);
DSE_CAPI_GEN int  dse_tree_get_seed(uint32_t e);
DSE_CAPI_GEN void dse_tree_set_seed(uint32_t e, int v);
DSE_CAPI_GEN float dse_tree_get_height_variation(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_height_variation(uint32_t e, float v);
DSE_CAPI_GEN int  dse_tree_get_random_rotation(uint32_t e);
DSE_CAPI_GEN void dse_tree_set_random_rotation(uint32_t e, int v);
DSE_CAPI_GEN float dse_tree_get_billboard_distance(uint32_t e);
DSE_CAPI_GEN void  dse_tree_set_billboard_distance(uint32_t e, float v);
DSE_CAPI_GEN void dse_tree_set_mesh_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_tree_get_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN void dse_tree_set_lod1_mesh_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_tree_get_lod1_mesh_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN void dse_tree_set_billboard_texture_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_tree_get_billboard_texture_path(uint32_t e, char* buf, int buf_size);

/* ---- TerrainTileManagerComponent ---- */
DSE_CAPI_GEN int  dse_terrain_tile_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_terrain_tile_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_terrain_tile_get_tile_world_size(uint32_t e);
DSE_CAPI_GEN void  dse_terrain_tile_set_tile_world_size(uint32_t e, float v);
DSE_CAPI_GEN int  dse_terrain_tile_get_tile_resolution(uint32_t e);
DSE_CAPI_GEN void dse_terrain_tile_set_tile_resolution(uint32_t e, int v);
DSE_CAPI_GEN float dse_terrain_tile_get_max_height(uint32_t e);
DSE_CAPI_GEN void  dse_terrain_tile_set_max_height(uint32_t e, float v);
DSE_CAPI_GEN float dse_terrain_tile_get_load_radius(uint32_t e);
DSE_CAPI_GEN void  dse_terrain_tile_set_load_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_terrain_tile_get_unload_radius(uint32_t e);
DSE_CAPI_GEN void  dse_terrain_tile_set_unload_radius(uint32_t e, float v);
DSE_CAPI_GEN int  dse_terrain_tile_get_use_procedural(uint32_t e);
DSE_CAPI_GEN void dse_terrain_tile_set_use_procedural(uint32_t e, int v);
DSE_CAPI_GEN float dse_terrain_tile_get_procedural_base_height(uint32_t e);
DSE_CAPI_GEN void  dse_terrain_tile_set_procedural_base_height(uint32_t e, float v);
DSE_CAPI_GEN int  dse_terrain_tile_get_max_lod_levels(uint32_t e);
DSE_CAPI_GEN void dse_terrain_tile_set_max_lod_levels(uint32_t e, int v);
DSE_CAPI_GEN float dse_terrain_tile_get_lod_distance_factor(uint32_t e);
DSE_CAPI_GEN void  dse_terrain_tile_set_lod_distance_factor(uint32_t e, float v);

/* ---- DynamicObstacleComponent ---- */
DSE_CAPI_GEN int  dse_dyn_obstacle_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_dyn_obstacle_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_dyn_obstacle_get_shape(uint32_t e);
DSE_CAPI_GEN void dse_dyn_obstacle_set_shape(uint32_t e, int v);
DSE_CAPI_GEN void dse_dyn_obstacle_get_box_extents(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_dyn_obstacle_set_box_extents(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_dyn_obstacle_get_cylinder_radius(uint32_t e);
DSE_CAPI_GEN void  dse_dyn_obstacle_set_cylinder_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_dyn_obstacle_get_cylinder_height(uint32_t e);
DSE_CAPI_GEN void  dse_dyn_obstacle_set_cylinder_height(uint32_t e, float v);

/* ---- NavMeshAutoRebakeComponent ---- */
DSE_CAPI_GEN int  dse_navmesh_rebake_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_navmesh_rebake_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_tile_size(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_tile_size(uint32_t e, float v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_rebake_cooldown(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_rebake_cooldown(uint32_t e, float v);
DSE_CAPI_GEN int  dse_navmesh_rebake_get_collect_terrain(uint32_t e);
DSE_CAPI_GEN void dse_navmesh_rebake_set_collect_terrain(uint32_t e, int v);
DSE_CAPI_GEN int  dse_navmesh_rebake_get_collect_mesh_renderers(uint32_t e);
DSE_CAPI_GEN void dse_navmesh_rebake_set_collect_mesh_renderers(uint32_t e, int v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_agent_height(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_agent_height(uint32_t e, float v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_agent_radius(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_agent_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_agent_max_climb(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_agent_max_climb(uint32_t e, float v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_agent_max_slope(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_agent_max_slope(uint32_t e, float v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_cell_size(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_cell_size(uint32_t e, float v);
DSE_CAPI_GEN float dse_navmesh_rebake_get_cell_height(uint32_t e);
DSE_CAPI_GEN void  dse_navmesh_rebake_set_cell_height(uint32_t e, float v);

/* ---- PostProcessComponent ---- */
DSE_CAPI_GEN int  dse_post_process_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_post_process_get_bloom_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_bloom_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_bloom_threshold(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_bloom_threshold(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_bloom_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_bloom_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_bloom_knee(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_bloom_knee(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_bloom_mip_weight(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_bloom_mip_weight(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_color_grading_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_color_grading_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_exposure(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_exposure(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_gamma(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_gamma(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_ssao_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_ssao_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_ssao_radius(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssao_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_ssao_bias(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssao_bias(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_ssao_sample_count(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_ssao_sample_count(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_ssao_power(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssao_power(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_ssao_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssao_intensity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_auto_exposure_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_auto_exposure_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_exposure_min(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_exposure_min(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_exposure_max(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_exposure_max(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_adaptation_speed_up(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_adaptation_speed_up(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_adaptation_speed_down(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_adaptation_speed_down(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_exposure_compensation(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_exposure_compensation(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_color_lut_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_color_lut_intensity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_vignette_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_vignette_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_vignette_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_vignette_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_vignette_radius(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_vignette_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_vignette_softness(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_vignette_softness(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_film_grain_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_film_grain_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_film_grain_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_film_grain_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_film_grain_time_scale(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_film_grain_time_scale(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_fxaa_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_fxaa_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_post_process_get_taa_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_taa_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_taa_blend_factor(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_taa_blend_factor(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_contact_shadow_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_contact_shadow_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_contact_shadow_strength(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_contact_shadow_strength(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_contact_shadow_steps(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_contact_shadow_steps(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_contact_shadow_step_size(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_contact_shadow_step_size(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_dof_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_dof_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_dof_focus_distance(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_dof_focus_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_dof_focus_range(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_dof_focus_range(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_dof_bokeh_radius(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_dof_bokeh_radius(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_motion_blur_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_motion_blur_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_motion_blur_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_motion_blur_intensity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_motion_blur_samples(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_motion_blur_samples(uint32_t e, int v);
DSE_CAPI_GEN int  dse_post_process_get_ssr_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_ssr_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_ssr_max_distance(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssr_max_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_ssr_thickness(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssr_thickness(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_ssr_step_size(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssr_step_size(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_ssr_max_steps(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_ssr_max_steps(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_ssr_fade_distance(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssr_fade_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_ssr_max_roughness(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_ssr_max_roughness(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_outline_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_outline_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_post_process_get_outline_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_post_process_set_outline_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_post_process_get_outline_thickness(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_outline_thickness(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_outline_depth_threshold(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_outline_depth_threshold(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_outline_normal_threshold(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_outline_normal_threshold(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_light_shaft_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_light_shaft_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_post_process_get_light_shaft_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_post_process_set_light_shaft_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_post_process_get_light_shaft_density(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_light_shaft_density(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_light_shaft_weight(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_light_shaft_weight(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_light_shaft_decay(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_light_shaft_decay(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_light_shaft_exposure(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_light_shaft_exposure(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_light_shaft_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_light_shaft_intensity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_light_shaft_samples(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_light_shaft_samples(uint32_t e, int v);
DSE_CAPI_GEN int  dse_post_process_get_fog_enabled(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_fog_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_post_process_get_fog_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_post_process_set_fog_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_post_process_get_fog_density(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_fog_density(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_fog_height_falloff(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_fog_height_falloff(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_fog_height_offset(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_fog_height_offset(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_fog_start(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_fog_start(uint32_t e, float v);
DSE_CAPI_GEN float dse_post_process_get_fog_end(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_fog_end(uint32_t e, float v);
DSE_CAPI_GEN int  dse_post_process_get_fog_steps(uint32_t e);
DSE_CAPI_GEN void dse_post_process_set_fog_steps(uint32_t e, int v);
DSE_CAPI_GEN float dse_post_process_get_fog_sun_scatter(uint32_t e);
DSE_CAPI_GEN void  dse_post_process_set_fog_sun_scatter(uint32_t e, float v);

/* ---- Animator3DComponent ---- */
DSE_CAPI_GEN int  dse_animator3d_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_animator3d_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_animator3d_set_danim_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_animator3d_get_danim_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN void dse_animator3d_set_dskel_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_animator3d_get_dskel_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN float dse_animator3d_get_speed(uint32_t e);
DSE_CAPI_GEN void  dse_animator3d_set_speed(uint32_t e, float v);
DSE_CAPI_GEN int  dse_animator3d_get_loop(uint32_t e);
DSE_CAPI_GEN void dse_animator3d_set_loop(uint32_t e, int v);
DSE_CAPI_GEN int  dse_animator3d_get_use_anim_tree(uint32_t e);
DSE_CAPI_GEN void dse_animator3d_set_use_anim_tree(uint32_t e, int v);
DSE_CAPI_GEN void dse_animator3d_set_blend_parameter(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_animator3d_get_blend_parameter(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN float dse_animator3d_get_blend_parameter_value(uint32_t e);
DSE_CAPI_GEN void  dse_animator3d_set_blend_parameter_value(uint32_t e, float v);

/* ---- DecalComponent ---- */
DSE_CAPI_GEN int  dse_decal_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_decal_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_decal_get_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_decal_set_color(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN float dse_decal_get_angle_fade(uint32_t e);
DSE_CAPI_GEN void  dse_decal_set_angle_fade(uint32_t e, float v);

/* ---- SkyboxComponent ---- */
DSE_CAPI_GEN int  dse_skybox_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_skybox_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_skybox_set_cubemap_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_skybox_get_cubemap_path(uint32_t e, char* buf, int buf_size);

/* ---- FreeCameraControllerComponent ---- */
DSE_CAPI_GEN int  dse_free_camera_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_free_camera_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_free_camera_get_move_speed(uint32_t e);
DSE_CAPI_GEN void  dse_free_camera_set_move_speed(uint32_t e, float v);
DSE_CAPI_GEN float dse_free_camera_get_mouse_sensitivity(uint32_t e);
DSE_CAPI_GEN void  dse_free_camera_set_mouse_sensitivity(uint32_t e, float v);
DSE_CAPI_GEN float dse_free_camera_get_pitch(uint32_t e);
DSE_CAPI_GEN void  dse_free_camera_set_pitch(uint32_t e, float v);
DSE_CAPI_GEN float dse_free_camera_get_yaw(uint32_t e);
DSE_CAPI_GEN void  dse_free_camera_set_yaw(uint32_t e, float v);

/* ---- SubSceneComponent ---- */
DSE_CAPI_GEN int  dse_sub_scene_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_sub_scene_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_sub_scene_set_scene_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_sub_scene_get_scene_path(uint32_t e, char* buf, int buf_size);

/* ---- BoundingBoxComponent ---- */
DSE_CAPI_GEN void dse_bounding_box_get_min_extents(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_bounding_box_set_min_extents(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_bounding_box_get_max_extents(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_bounding_box_set_max_extents(uint32_t e, float x, float y, float z);

/* ---- WaterComponent ---- */
DSE_CAPI_GEN int  dse_water_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_water_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_water_get_water_level(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_water_level(uint32_t e, float v);
DSE_CAPI_GEN void dse_water_get_deep_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_water_set_deep_color(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN void dse_water_get_shallow_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_water_set_shallow_color(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN float dse_water_get_max_depth(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_max_depth(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_transparency(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_transparency(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_wave_amplitude(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_wave_amplitude(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_wave_frequency(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_wave_frequency(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_wave_speed(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_wave_speed(uint32_t e, float v);
DSE_CAPI_GEN void dse_water_get_wave_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_water_set_wave_direction(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_water_get_refraction_strength(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_refraction_strength(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_reflection_strength(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_reflection_strength(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_specular_power(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_specular_power(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_caustic_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_caustic_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_caustic_scale(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_caustic_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_foam_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_foam_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_foam_depth_threshold(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_foam_depth_threshold(uint32_t e, float v);
DSE_CAPI_GEN float dse_water_get_underwater_fog_density(uint32_t e);
DSE_CAPI_GEN void  dse_water_set_underwater_fog_density(uint32_t e, float v);
DSE_CAPI_GEN void dse_water_get_underwater_fog_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_water_set_underwater_fog_color(uint32_t e, float x, float y, float z, float w);

/* ---- LightProbeComponent ---- */
DSE_CAPI_GEN int  dse_light_probe_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_light_probe_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_light_probe_get_influence_radius(uint32_t e);
DSE_CAPI_GEN void  dse_light_probe_set_influence_radius(uint32_t e, float v);
DSE_CAPI_GEN int  dse_light_probe_get_show_debug(uint32_t e);
DSE_CAPI_GEN void dse_light_probe_set_show_debug(uint32_t e, int v);

/* ---- ReflectionProbeComponent ---- */
DSE_CAPI_GEN int  dse_reflection_probe_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_reflection_probe_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_reflection_probe_get_influence_radius(uint32_t e);
DSE_CAPI_GEN void  dse_reflection_probe_set_influence_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_reflection_probe_get_box_size_x(uint32_t e);
DSE_CAPI_GEN void  dse_reflection_probe_set_box_size_x(uint32_t e, float v);
DSE_CAPI_GEN float dse_reflection_probe_get_box_size_y(uint32_t e);
DSE_CAPI_GEN void  dse_reflection_probe_set_box_size_y(uint32_t e, float v);
DSE_CAPI_GEN float dse_reflection_probe_get_box_size_z(uint32_t e);
DSE_CAPI_GEN void  dse_reflection_probe_set_box_size_z(uint32_t e, float v);
DSE_CAPI_GEN int  dse_reflection_probe_get_use_box_projection(uint32_t e);
DSE_CAPI_GEN void dse_reflection_probe_set_use_box_projection(uint32_t e, int v);
DSE_CAPI_GEN int  dse_reflection_probe_get_resolution(uint32_t e);
DSE_CAPI_GEN void dse_reflection_probe_set_resolution(uint32_t e, int v);
DSE_CAPI_GEN int  dse_reflection_probe_get_show_debug(uint32_t e);
DSE_CAPI_GEN void dse_reflection_probe_set_show_debug(uint32_t e, int v);

/* ---- GIProbeVolumeComponent ---- */
DSE_CAPI_GEN int  dse_gi_probe_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_gi_probe_get_origin(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_gi_probe_set_origin(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_gi_probe_get_extent(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_gi_probe_set_extent(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_gi_probe_get_resolution_x(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_resolution_x(uint32_t e, int v);
DSE_CAPI_GEN int  dse_gi_probe_get_resolution_y(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_resolution_y(uint32_t e, int v);
DSE_CAPI_GEN int  dse_gi_probe_get_resolution_z(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_resolution_z(uint32_t e, int v);
DSE_CAPI_GEN int  dse_gi_probe_get_irradiance_texels(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_irradiance_texels(uint32_t e, int v);
DSE_CAPI_GEN int  dse_gi_probe_get_visibility_texels(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_visibility_texels(uint32_t e, int v);
DSE_CAPI_GEN int  dse_gi_probe_get_rays_per_probe(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_rays_per_probe(uint32_t e, int v);
DSE_CAPI_GEN float dse_gi_probe_get_hysteresis(uint32_t e);
DSE_CAPI_GEN void  dse_gi_probe_set_hysteresis(uint32_t e, float v);
DSE_CAPI_GEN float dse_gi_probe_get_gi_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_gi_probe_set_gi_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_gi_probe_get_normal_bias(uint32_t e);
DSE_CAPI_GEN void  dse_gi_probe_set_normal_bias(uint32_t e, float v);
DSE_CAPI_GEN float dse_gi_probe_get_view_bias(uint32_t e);
DSE_CAPI_GEN void  dse_gi_probe_set_view_bias(uint32_t e, float v);
DSE_CAPI_GEN int  dse_gi_probe_get_show_debug_probes(uint32_t e);
DSE_CAPI_GEN void dse_gi_probe_set_show_debug_probes(uint32_t e, int v);

/* ---- FoliageComponent ---- */
DSE_CAPI_GEN int  dse_foliage_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_foliage_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_foliage_get_wind_strength(uint32_t e);
DSE_CAPI_GEN void  dse_foliage_set_wind_strength(uint32_t e, float v);
DSE_CAPI_GEN float dse_foliage_get_stiffness(uint32_t e);
DSE_CAPI_GEN void  dse_foliage_set_stiffness(uint32_t e, float v);
DSE_CAPI_GEN float dse_foliage_get_phase_offset(uint32_t e);
DSE_CAPI_GEN void  dse_foliage_set_phase_offset(uint32_t e, float v);
DSE_CAPI_GEN float dse_foliage_get_push_response(uint32_t e);
DSE_CAPI_GEN void  dse_foliage_set_push_response(uint32_t e, float v);

/* ---- RigidBody3DComponent ---- */
DSE_CAPI_GEN int  dse_rigidbody3d_get_type(uint32_t e);
DSE_CAPI_GEN void dse_rigidbody3d_set_type(uint32_t e, int v);
DSE_CAPI_GEN float dse_rigidbody3d_get_mass(uint32_t e);
DSE_CAPI_GEN void  dse_rigidbody3d_set_mass(uint32_t e, float v);
DSE_CAPI_GEN float dse_rigidbody3d_get_drag(uint32_t e);
DSE_CAPI_GEN void  dse_rigidbody3d_set_drag(uint32_t e, float v);
DSE_CAPI_GEN float dse_rigidbody3d_get_angular_drag(uint32_t e);
DSE_CAPI_GEN void  dse_rigidbody3d_set_angular_drag(uint32_t e, float v);
DSE_CAPI_GEN int  dse_rigidbody3d_get_use_gravity(uint32_t e);
DSE_CAPI_GEN void dse_rigidbody3d_set_use_gravity(uint32_t e, int v);
DSE_CAPI_GEN float dse_rigidbody3d_get_gravity_scale(uint32_t e);
DSE_CAPI_GEN void  dse_rigidbody3d_set_gravity_scale(uint32_t e, float v);
DSE_CAPI_GEN int  dse_rigidbody3d_get_is_kinematic(uint32_t e);
DSE_CAPI_GEN void dse_rigidbody3d_set_is_kinematic(uint32_t e, int v);
DSE_CAPI_GEN int  dse_rigidbody3d_get_collision_layer(uint32_t e);
DSE_CAPI_GEN void dse_rigidbody3d_set_collision_layer(uint32_t e, int v);
DSE_CAPI_GEN int  dse_rigidbody3d_get_collision_mask(uint32_t e);
DSE_CAPI_GEN void dse_rigidbody3d_set_collision_mask(uint32_t e, int v);

/* ---- BoxCollider3DComponent ---- */
DSE_CAPI_GEN void dse_box_collider3d_get_size(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_box_collider3d_set_size(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_box_collider3d_get_center(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_box_collider3d_set_center(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_box_collider3d_get_is_trigger(uint32_t e);
DSE_CAPI_GEN void dse_box_collider3d_set_is_trigger(uint32_t e, int v);
DSE_CAPI_GEN float dse_box_collider3d_get_bounciness(uint32_t e);
DSE_CAPI_GEN void  dse_box_collider3d_set_bounciness(uint32_t e, float v);
DSE_CAPI_GEN float dse_box_collider3d_get_friction(uint32_t e);
DSE_CAPI_GEN void  dse_box_collider3d_set_friction(uint32_t e, float v);

/* ---- SphereCollider3DComponent ---- */
DSE_CAPI_GEN float dse_sphere_collider3d_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_sphere_collider3d_set_radius(uint32_t e, float v);
DSE_CAPI_GEN void dse_sphere_collider3d_get_center(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_sphere_collider3d_set_center(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_sphere_collider3d_get_is_trigger(uint32_t e);
DSE_CAPI_GEN void dse_sphere_collider3d_set_is_trigger(uint32_t e, int v);
DSE_CAPI_GEN float dse_sphere_collider3d_get_bounciness(uint32_t e);
DSE_CAPI_GEN void  dse_sphere_collider3d_set_bounciness(uint32_t e, float v);
DSE_CAPI_GEN float dse_sphere_collider3d_get_friction(uint32_t e);
DSE_CAPI_GEN void  dse_sphere_collider3d_set_friction(uint32_t e, float v);

/* ---- CapsuleCollider3DComponent ---- */
DSE_CAPI_GEN float dse_capsule_collider3d_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_capsule_collider3d_set_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_capsule_collider3d_get_height(uint32_t e);
DSE_CAPI_GEN void  dse_capsule_collider3d_set_height(uint32_t e, float v);
DSE_CAPI_GEN void dse_capsule_collider3d_get_center(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_capsule_collider3d_set_center(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_capsule_collider3d_get_direction(uint32_t e);
DSE_CAPI_GEN void dse_capsule_collider3d_set_direction(uint32_t e, int v);
DSE_CAPI_GEN int  dse_capsule_collider3d_get_is_trigger(uint32_t e);
DSE_CAPI_GEN void dse_capsule_collider3d_set_is_trigger(uint32_t e, int v);
DSE_CAPI_GEN float dse_capsule_collider3d_get_bounciness(uint32_t e);
DSE_CAPI_GEN void  dse_capsule_collider3d_set_bounciness(uint32_t e, float v);
DSE_CAPI_GEN float dse_capsule_collider3d_get_friction(uint32_t e);
DSE_CAPI_GEN void  dse_capsule_collider3d_set_friction(uint32_t e, float v);

/* ---- MeshCollider3DComponent ---- */
DSE_CAPI_GEN int  dse_mesh_collider3d_get_convex(uint32_t e);
DSE_CAPI_GEN void dse_mesh_collider3d_set_convex(uint32_t e, int v);
DSE_CAPI_GEN int  dse_mesh_collider3d_get_is_trigger(uint32_t e);
DSE_CAPI_GEN void dse_mesh_collider3d_set_is_trigger(uint32_t e, int v);
DSE_CAPI_GEN float dse_mesh_collider3d_get_bounciness(uint32_t e);
DSE_CAPI_GEN void  dse_mesh_collider3d_set_bounciness(uint32_t e, float v);
DSE_CAPI_GEN float dse_mesh_collider3d_get_friction(uint32_t e);
DSE_CAPI_GEN void  dse_mesh_collider3d_set_friction(uint32_t e, float v);

/* ---- CharacterController3DComponent ---- */
DSE_CAPI_GEN float dse_character_ctrl3d_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_character_ctrl3d_set_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_ctrl3d_get_height(uint32_t e);
DSE_CAPI_GEN void  dse_character_ctrl3d_set_height(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_ctrl3d_get_slope_limit(uint32_t e);
DSE_CAPI_GEN void  dse_character_ctrl3d_set_slope_limit(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_ctrl3d_get_step_offset(uint32_t e);
DSE_CAPI_GEN void  dse_character_ctrl3d_set_step_offset(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_ctrl3d_get_skin_width(uint32_t e);
DSE_CAPI_GEN void  dse_character_ctrl3d_set_skin_width(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_ctrl3d_get_min_move_distance(uint32_t e);
DSE_CAPI_GEN void  dse_character_ctrl3d_set_min_move_distance(uint32_t e, float v);

/* ---- Joint3DComponent ---- */
DSE_CAPI_GEN int  dse_joint3d_get_type(uint32_t e);
DSE_CAPI_GEN void dse_joint3d_set_type(uint32_t e, int v);
DSE_CAPI_GEN int  dse_joint3d_get_connected_entity_id(uint32_t e);
DSE_CAPI_GEN void dse_joint3d_set_connected_entity_id(uint32_t e, int v);
DSE_CAPI_GEN void dse_joint3d_get_anchor(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_joint3d_set_anchor(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_joint3d_get_connected_anchor(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_joint3d_set_connected_anchor(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_joint3d_get_axis(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_joint3d_set_axis(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_joint3d_get_use_limits(uint32_t e);
DSE_CAPI_GEN void dse_joint3d_set_use_limits(uint32_t e, int v);
DSE_CAPI_GEN float dse_joint3d_get_lower_limit(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_lower_limit(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_upper_limit(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_upper_limit(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_min_distance(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_min_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_max_distance(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_max_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_spring_stiffness(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_spring_stiffness(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_spring_damping(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_spring_damping(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_break_force(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_break_force(uint32_t e, float v);
DSE_CAPI_GEN float dse_joint3d_get_break_torque(uint32_t e);
DSE_CAPI_GEN void  dse_joint3d_set_break_torque(uint32_t e, float v);

/* ---- RagdollComponent ---- */
DSE_CAPI_GEN int  dse_ragdoll_get_active(uint32_t e);
DSE_CAPI_GEN void dse_ragdoll_set_active(uint32_t e, int v);
DSE_CAPI_GEN int  dse_ragdoll_get_auto_setup(uint32_t e);
DSE_CAPI_GEN void dse_ragdoll_set_auto_setup(uint32_t e, int v);
DSE_CAPI_GEN float dse_ragdoll_get_total_mass(uint32_t e);
DSE_CAPI_GEN void  dse_ragdoll_set_total_mass(uint32_t e, float v);
DSE_CAPI_GEN float dse_ragdoll_get_joint_stiffness(uint32_t e);
DSE_CAPI_GEN void  dse_ragdoll_set_joint_stiffness(uint32_t e, float v);
DSE_CAPI_GEN float dse_ragdoll_get_joint_damping(uint32_t e);
DSE_CAPI_GEN void  dse_ragdoll_set_joint_damping(uint32_t e, float v);
DSE_CAPI_GEN int  dse_ragdoll_get_collision_layer(uint32_t e);
DSE_CAPI_GEN void dse_ragdoll_set_collision_layer(uint32_t e, int v);
DSE_CAPI_GEN int  dse_ragdoll_get_collision_mask(uint32_t e);
DSE_CAPI_GEN void dse_ragdoll_set_collision_mask(uint32_t e, int v);

/* ---- SoftBodyComponent ---- */
DSE_CAPI_GEN int  dse_soft_body_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_soft_body_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_soft_body_get_stiffness(uint32_t e);
DSE_CAPI_GEN void  dse_soft_body_set_stiffness(uint32_t e, float v);
DSE_CAPI_GEN int  dse_soft_body_get_solver_iterations(uint32_t e);
DSE_CAPI_GEN void dse_soft_body_set_solver_iterations(uint32_t e, int v);
DSE_CAPI_GEN float dse_soft_body_get_damping(uint32_t e);
DSE_CAPI_GEN void  dse_soft_body_set_damping(uint32_t e, float v);
DSE_CAPI_GEN int  dse_soft_body_get_use_gravity(uint32_t e);
DSE_CAPI_GEN void dse_soft_body_set_use_gravity(uint32_t e, int v);
DSE_CAPI_GEN float dse_soft_body_get_gravity_scale(uint32_t e);
DSE_CAPI_GEN void  dse_soft_body_set_gravity_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_soft_body_get_volume_stiffness(uint32_t e);
DSE_CAPI_GEN void  dse_soft_body_set_volume_stiffness(uint32_t e, float v);

/* ---- VehicleComponent ---- */
DSE_CAPI_GEN int  dse_vehicle_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_vehicle_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_vehicle_get_max_engine_force(uint32_t e);
DSE_CAPI_GEN void  dse_vehicle_set_max_engine_force(uint32_t e, float v);
DSE_CAPI_GEN float dse_vehicle_get_max_brake_force(uint32_t e);
DSE_CAPI_GEN void  dse_vehicle_set_max_brake_force(uint32_t e, float v);
DSE_CAPI_GEN float dse_vehicle_get_max_steer_angle(uint32_t e);
DSE_CAPI_GEN void  dse_vehicle_set_max_steer_angle(uint32_t e, float v);

/* ---- RopeComponent ---- */
DSE_CAPI_GEN int  dse_rope_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_rope_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_rope_get_segment_count(uint32_t e);
DSE_CAPI_GEN void dse_rope_set_segment_count(uint32_t e, int v);
DSE_CAPI_GEN float dse_rope_get_segment_length(uint32_t e);
DSE_CAPI_GEN void  dse_rope_set_segment_length(uint32_t e, float v);
DSE_CAPI_GEN float dse_rope_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_rope_set_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_rope_get_damping(uint32_t e);
DSE_CAPI_GEN void  dse_rope_set_damping(uint32_t e, float v);
DSE_CAPI_GEN int  dse_rope_get_solver_iterations(uint32_t e);
DSE_CAPI_GEN void dse_rope_set_solver_iterations(uint32_t e, int v);
DSE_CAPI_GEN int  dse_rope_get_use_gravity(uint32_t e);
DSE_CAPI_GEN void dse_rope_set_use_gravity(uint32_t e, int v);
DSE_CAPI_GEN float dse_rope_get_gravity_scale(uint32_t e);
DSE_CAPI_GEN void  dse_rope_set_gravity_scale(uint32_t e, float v);
DSE_CAPI_GEN int  dse_rope_get_anchor_entity_a(uint32_t e);
DSE_CAPI_GEN void dse_rope_set_anchor_entity_a(uint32_t e, int v);
DSE_CAPI_GEN int  dse_rope_get_anchor_entity_b(uint32_t e);
DSE_CAPI_GEN void dse_rope_set_anchor_entity_b(uint32_t e, int v);
DSE_CAPI_GEN void dse_rope_get_anchor_offset_a(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_rope_set_anchor_offset_a(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_rope_get_anchor_offset_b(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_rope_set_anchor_offset_b(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_rope_get_start_position(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_rope_set_start_position(uint32_t e, float x, float y, float z);

/* ---- BuoyancyComponent ---- */
DSE_CAPI_GEN int  dse_buoyancy_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_buoyancy_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_buoyancy_get_water_level(uint32_t e);
DSE_CAPI_GEN void  dse_buoyancy_set_water_level(uint32_t e, float v);
DSE_CAPI_GEN int  dse_buoyancy_get_use_fluid_system(uint32_t e);
DSE_CAPI_GEN void dse_buoyancy_set_use_fluid_system(uint32_t e, int v);
DSE_CAPI_GEN float dse_buoyancy_get_buoyancy_force(uint32_t e);
DSE_CAPI_GEN void  dse_buoyancy_set_buoyancy_force(uint32_t e, float v);
DSE_CAPI_GEN float dse_buoyancy_get_water_drag(uint32_t e);
DSE_CAPI_GEN void  dse_buoyancy_set_water_drag(uint32_t e, float v);
DSE_CAPI_GEN float dse_buoyancy_get_water_angular_drag(uint32_t e);
DSE_CAPI_GEN void  dse_buoyancy_set_water_angular_drag(uint32_t e, float v);
DSE_CAPI_GEN float dse_buoyancy_get_submerge_depth(uint32_t e);
DSE_CAPI_GEN void  dse_buoyancy_set_submerge_depth(uint32_t e, float v);

/* ---- AtmosphereComponent ---- */
DSE_CAPI_GEN int  dse_atmosphere_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_atmosphere_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_atmosphere_get_planet_radius(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_planet_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_atmosphere_height(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_atmosphere_height(uint32_t e, float v);
DSE_CAPI_GEN void dse_atmosphere_get_rayleigh_coeff(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_atmosphere_set_rayleigh_coeff(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_atmosphere_get_rayleigh_scale_height(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_rayleigh_scale_height(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_mie_coeff(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_mie_coeff(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_mie_scale_height(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_mie_scale_height(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_mie_g(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_mie_g(uint32_t e, float v);
DSE_CAPI_GEN void dse_atmosphere_get_mie_albedo(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_atmosphere_set_mie_albedo(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN void dse_atmosphere_get_ozone_coeff(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_atmosphere_set_ozone_coeff(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_atmosphere_get_ozone_center_h(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_ozone_center_h(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_ozone_width(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_ozone_width(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_sun_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_sun_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_atmosphere_get_sun_disk_angle(uint32_t e);
DSE_CAPI_GEN void  dse_atmosphere_set_sun_disk_angle(uint32_t e, float v);
DSE_CAPI_GEN int  dse_atmosphere_get_aerial_perspective_enabled(uint32_t e);
DSE_CAPI_GEN void dse_atmosphere_set_aerial_perspective_enabled(uint32_t e, int v);

/* ---- VolumetricCloudComponent ---- */
DSE_CAPI_GEN int  dse_volumetric_cloud_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_volumetric_cloud_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_cloud_bottom(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_cloud_bottom(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_cloud_top(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_cloud_top(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_coverage(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_coverage(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_density(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_density(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_shape_scale(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_shape_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_detail_scale(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_detail_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_detail_strength(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_detail_strength(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_erosion(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_erosion(uint32_t e, float v);
DSE_CAPI_GEN void dse_volumetric_cloud_get_wind_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_volumetric_cloud_set_wind_direction(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_volumetric_cloud_get_wind_speed(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_wind_speed(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_silver_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_silver_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_silver_spread(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_silver_spread(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_powder_strength(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_powder_strength(uint32_t e, float v);
DSE_CAPI_GEN float dse_volumetric_cloud_get_ambient_strength(uint32_t e);
DSE_CAPI_GEN void  dse_volumetric_cloud_set_ambient_strength(uint32_t e, float v);
DSE_CAPI_GEN int  dse_volumetric_cloud_get_half_resolution(uint32_t e);
DSE_CAPI_GEN void dse_volumetric_cloud_set_half_resolution(uint32_t e, int v);
DSE_CAPI_GEN int  dse_volumetric_cloud_get_temporal_reprojection(uint32_t e);
DSE_CAPI_GEN void dse_volumetric_cloud_set_temporal_reprojection(uint32_t e, int v);
DSE_CAPI_GEN int  dse_volumetric_cloud_get_cloud_shadow_enabled(uint32_t e);
DSE_CAPI_GEN void dse_volumetric_cloud_set_cloud_shadow_enabled(uint32_t e, int v);

/* ---- DayNightCycleComponent ---- */
DSE_CAPI_GEN int  dse_day_night_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_day_night_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_day_night_get_time_of_day(uint32_t e);
DSE_CAPI_GEN void  dse_day_night_set_time_of_day(uint32_t e, float v);
DSE_CAPI_GEN float dse_day_night_get_time_speed(uint32_t e);
DSE_CAPI_GEN void  dse_day_night_set_time_speed(uint32_t e, float v);
DSE_CAPI_GEN int  dse_day_night_get_auto_advance(uint32_t e);
DSE_CAPI_GEN void dse_day_night_set_auto_advance(uint32_t e, int v);
DSE_CAPI_GEN float dse_day_night_get_latitude(uint32_t e);
DSE_CAPI_GEN void  dse_day_night_set_latitude(uint32_t e, float v);
DSE_CAPI_GEN float dse_day_night_get_longitude(uint32_t e);
DSE_CAPI_GEN void  dse_day_night_set_longitude(uint32_t e, float v);
DSE_CAPI_GEN int  dse_day_night_get_day_of_year(uint32_t e);
DSE_CAPI_GEN void dse_day_night_set_day_of_year(uint32_t e, int v);

/* ---- HairComponent ---- */
DSE_CAPI_GEN int  dse_hair_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_hair_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_hair_set_hair_asset_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_hair_get_hair_asset_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN float dse_hair_get_damping(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_damping(uint32_t e, float v);
DSE_CAPI_GEN float dse_hair_get_stiffness_local(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_stiffness_local(uint32_t e, float v);
DSE_CAPI_GEN float dse_hair_get_stiffness_global(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_stiffness_global(uint32_t e, float v);
DSE_CAPI_GEN float dse_hair_get_gravity(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_gravity(uint32_t e, float v);
DSE_CAPI_GEN void dse_hair_get_wind(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_hair_set_wind(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_hair_get_wind_turbulence(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_wind_turbulence(uint32_t e, float v);
DSE_CAPI_GEN void dse_hair_get_root_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_hair_set_root_color(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN void dse_hair_get_tip_color(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_hair_set_tip_color(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN float dse_hair_get_fiber_radius(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_fiber_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_hair_get_opacity(uint32_t e);
DSE_CAPI_GEN void  dse_hair_set_opacity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_hair_get_cast_shadow(uint32_t e);
DSE_CAPI_GEN void dse_hair_set_cast_shadow(uint32_t e, int v);
DSE_CAPI_GEN int  dse_hair_get_receive_shadow(uint32_t e);
DSE_CAPI_GEN void dse_hair_set_receive_shadow(uint32_t e, int v);

/* ---- ImpostorComponent ---- */
DSE_CAPI_GEN int  dse_impostor_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_impostor_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_impostor_set_atlas_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_impostor_get_atlas_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN int  dse_impostor_get_frames_x(uint32_t e);
DSE_CAPI_GEN void dse_impostor_set_frames_x(uint32_t e, int v);
DSE_CAPI_GEN int  dse_impostor_get_frames_y(uint32_t e);
DSE_CAPI_GEN void dse_impostor_set_frames_y(uint32_t e, int v);
DSE_CAPI_GEN float dse_impostor_get_transition_distance(uint32_t e);
DSE_CAPI_GEN void  dse_impostor_set_transition_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_impostor_get_fade_range(uint32_t e);
DSE_CAPI_GEN void  dse_impostor_set_fade_range(uint32_t e, float v);
DSE_CAPI_GEN float dse_impostor_get_cull_distance(uint32_t e);
DSE_CAPI_GEN void  dse_impostor_set_cull_distance(uint32_t e, float v);
DSE_CAPI_GEN float dse_impostor_get_impostor_size(uint32_t e);
DSE_CAPI_GEN void  dse_impostor_set_impostor_size(uint32_t e, float v);
DSE_CAPI_GEN void dse_impostor_get_pivot_offset(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_impostor_set_pivot_offset(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_impostor_get_cast_shadow(uint32_t e);
DSE_CAPI_GEN void dse_impostor_set_cast_shadow(uint32_t e, int v);
DSE_CAPI_GEN int  dse_impostor_get_use_frame_interpolation(uint32_t e);
DSE_CAPI_GEN void dse_impostor_set_use_frame_interpolation(uint32_t e, int v);
DSE_CAPI_GEN float dse_impostor_get_normal_strength(uint32_t e);
DSE_CAPI_GEN void  dse_impostor_set_normal_strength(uint32_t e, float v);
DSE_CAPI_GEN int  dse_impostor_get_auto_from_lod_group(uint32_t e);
DSE_CAPI_GEN void dse_impostor_set_auto_from_lod_group(uint32_t e, int v);

/* ---- StreamingOriginComponent ---- */
DSE_CAPI_GEN int  dse_streaming_origin_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_streaming_origin_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_streaming_origin_get_load_radius(uint32_t e);
DSE_CAPI_GEN void  dse_streaming_origin_set_load_radius(uint32_t e, float v);
DSE_CAPI_GEN float dse_streaming_origin_get_unload_radius(uint32_t e);
DSE_CAPI_GEN void  dse_streaming_origin_set_unload_radius(uint32_t e, float v);

/* ---- WorldPartitionConfigComponent ---- */
DSE_CAPI_GEN int  dse_world_partition_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_world_partition_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_world_partition_get_cell_size(uint32_t e);
DSE_CAPI_GEN void  dse_world_partition_set_cell_size(uint32_t e, float v);
DSE_CAPI_GEN void dse_world_partition_set_cells_directory(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_world_partition_get_cells_directory(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN int  dse_world_partition_get_grid_min_x(uint32_t e);
DSE_CAPI_GEN void dse_world_partition_set_grid_min_x(uint32_t e, int v);
DSE_CAPI_GEN int  dse_world_partition_get_grid_max_x(uint32_t e);
DSE_CAPI_GEN void dse_world_partition_set_grid_max_x(uint32_t e, int v);
DSE_CAPI_GEN int  dse_world_partition_get_grid_min_y(uint32_t e);
DSE_CAPI_GEN void dse_world_partition_set_grid_min_y(uint32_t e, int v);
DSE_CAPI_GEN int  dse_world_partition_get_grid_max_y(uint32_t e);
DSE_CAPI_GEN void dse_world_partition_set_grid_max_y(uint32_t e, int v);
DSE_CAPI_GEN int  dse_world_partition_get_max_loads_per_frame(uint32_t e);
DSE_CAPI_GEN void dse_world_partition_set_max_loads_per_frame(uint32_t e, int v);

/* ---- HLODConfigComponent ---- */
DSE_CAPI_GEN int  dse_hlod_config_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_hlod_config_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_hlod_config_set_hlod_data_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_hlod_config_get_hlod_data_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN float dse_hlod_config_get_distance_scale(uint32_t e);
DSE_CAPI_GEN void  dse_hlod_config_set_distance_scale(uint32_t e, float v);
DSE_CAPI_GEN float dse_hlod_config_get_hysteresis(uint32_t e);
DSE_CAPI_GEN void  dse_hlod_config_set_hysteresis(uint32_t e, float v);

/* ---- VirtualTextureComponent ---- */
DSE_CAPI_GEN int  dse_virtual_texture_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_virtual_texture_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN int  dse_virtual_texture_get_vt_id(uint32_t e);
DSE_CAPI_GEN void dse_virtual_texture_set_vt_id(uint32_t e, int v);
DSE_CAPI_GEN void dse_virtual_texture_set_tile_data_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_virtual_texture_get_tile_data_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN int  dse_virtual_texture_get_virtual_width(uint32_t e);
DSE_CAPI_GEN void dse_virtual_texture_set_virtual_width(uint32_t e, int v);
DSE_CAPI_GEN int  dse_virtual_texture_get_virtual_height(uint32_t e);
DSE_CAPI_GEN void dse_virtual_texture_set_virtual_height(uint32_t e, int v);
DSE_CAPI_GEN float dse_virtual_texture_get_mip_bias(uint32_t e);
DSE_CAPI_GEN void  dse_virtual_texture_set_mip_bias(uint32_t e, float v);

/* ---- LightmapComponent ---- */
DSE_CAPI_GEN void dse_lightmap_set_lightmap_path(uint32_t e, const char* v);
DSE_CAPI_GEN int  dse_lightmap_get_lightmap_path(uint32_t e, char* buf, int buf_size);
DSE_CAPI_GEN float dse_lightmap_get_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_lightmap_set_intensity(uint32_t e, float v);
DSE_CAPI_GEN void dse_lightmap_get_st_offset(uint32_t e, float* x, float* y, float* z, float* w);
DSE_CAPI_GEN void dse_lightmap_set_st_offset(uint32_t e, float x, float y, float z, float w);
DSE_CAPI_GEN int  dse_lightmap_get_use_ao(uint32_t e);
DSE_CAPI_GEN void dse_lightmap_set_use_ao(uint32_t e, int v);

/* ---- CharacterMovementConfig ---- */
DSE_CAPI_GEN int  dse_character_movement_cfg_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_character_movement_cfg_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_max_walk_speed(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_max_walk_speed(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_max_sprint_speed(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_max_sprint_speed(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_max_crouch_speed(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_max_crouch_speed(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_ground_acceleration(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_ground_acceleration(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_ground_deceleration(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_ground_deceleration(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_ground_friction(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_ground_friction(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_gravity(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_gravity(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_jump_velocity(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_jump_velocity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_character_movement_cfg_get_max_jump_count(uint32_t e);
DSE_CAPI_GEN void dse_character_movement_cfg_set_max_jump_count(uint32_t e, int v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_coyote_time(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_coyote_time(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_jump_buffer_time(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_jump_buffer_time(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_air_control(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_air_control(uint32_t e, float v);
DSE_CAPI_GEN float dse_character_movement_cfg_get_rotation_rate(uint32_t e);
DSE_CAPI_GEN void  dse_character_movement_cfg_set_rotation_rate(uint32_t e, float v);
DSE_CAPI_GEN int  dse_character_movement_cfg_get_publish_events(uint32_t e);
DSE_CAPI_GEN void dse_character_movement_cfg_set_publish_events(uint32_t e, int v);

/* ---- CharacterMovementState ---- */
DSE_CAPI_GEN void dse_character_movement_get_input_direction(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_character_movement_set_input_direction(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN int  dse_character_movement_get_input_jump(uint32_t e);
DSE_CAPI_GEN void dse_character_movement_set_input_jump(uint32_t e, int v);
DSE_CAPI_GEN int  dse_character_movement_get_input_sprint(uint32_t e);
DSE_CAPI_GEN void dse_character_movement_set_input_sprint(uint32_t e, int v);
DSE_CAPI_GEN int  dse_character_movement_get_input_crouch(uint32_t e);
DSE_CAPI_GEN void dse_character_movement_set_input_crouch(uint32_t e, int v);
DSE_CAPI_GEN void dse_character_movement_get_velocity(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN int  dse_character_movement_get_is_grounded(uint32_t e);
DSE_CAPI_GEN int  dse_character_movement_get_is_jumping(uint32_t e);
DSE_CAPI_GEN int  dse_character_movement_get_jump_count(uint32_t e);

/* ---- SpringArm3DComponent ---- */
DSE_CAPI_GEN int  dse_spring_arm_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_spring_arm_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN void dse_spring_arm_get_target_offset(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_spring_arm_set_target_offset(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_spring_arm_get_arm_length(uint32_t e);
DSE_CAPI_GEN void  dse_spring_arm_set_arm_length(uint32_t e, float v);
DSE_CAPI_GEN int  dse_spring_arm_get_collision_test(uint32_t e);
DSE_CAPI_GEN void dse_spring_arm_set_collision_test(uint32_t e, int v);
DSE_CAPI_GEN float dse_spring_arm_get_pitch(uint32_t e);
DSE_CAPI_GEN void  dse_spring_arm_set_pitch(uint32_t e, float v);
DSE_CAPI_GEN float dse_spring_arm_get_yaw(uint32_t e);
DSE_CAPI_GEN void  dse_spring_arm_set_yaw(uint32_t e, float v);
DSE_CAPI_GEN float dse_spring_arm_get_position_lag_speed(uint32_t e);
DSE_CAPI_GEN void  dse_spring_arm_set_position_lag_speed(uint32_t e, float v);
DSE_CAPI_GEN float dse_spring_arm_get_shake_trauma(uint32_t e);
DSE_CAPI_GEN void  dse_spring_arm_set_shake_trauma(uint32_t e, float v);

/* ---- PlayerControllerComponent ---- */
DSE_CAPI_GEN int  dse_player_controller_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_player_controller_set_enabled(uint32_t e, int v);
DSE_CAPI_GEN float dse_player_controller_get_mouse_sensitivity(uint32_t e);
DSE_CAPI_GEN void  dse_player_controller_set_mouse_sensitivity(uint32_t e, float v);
DSE_CAPI_GEN float dse_player_controller_get_gamepad_sensitivity(uint32_t e);
DSE_CAPI_GEN void  dse_player_controller_set_gamepad_sensitivity(uint32_t e, float v);
DSE_CAPI_GEN int  dse_player_controller_get_invert_y(uint32_t e);
DSE_CAPI_GEN void dse_player_controller_set_invert_y(uint32_t e, int v);
DSE_CAPI_GEN float dse_player_controller_get_stick_dead_zone(uint32_t e);
DSE_CAPI_GEN void  dse_player_controller_set_stick_dead_zone(uint32_t e, float v);
DSE_CAPI_GEN float dse_player_controller_get_move_response_curve(uint32_t e);
DSE_CAPI_GEN void  dse_player_controller_set_move_response_curve(uint32_t e, float v);
DSE_CAPI_GEN float dse_player_controller_get_look_response_curve(uint32_t e);
DSE_CAPI_GEN void  dse_player_controller_set_look_response_curve(uint32_t e, float v);

#ifdef __cplusplus
}
#endif

#endif // DSE_API_H not yet included
