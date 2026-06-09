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

/* ---- PointLightComponent ---- */
DSE_CAPI_GEN void dse_point_light_get_color(uint32_t e, float* x, float* y, float* z);
DSE_CAPI_GEN void dse_point_light_set_color(uint32_t e, float x, float y, float z);
DSE_CAPI_GEN float dse_point_light_get_intensity(uint32_t e);
DSE_CAPI_GEN void  dse_point_light_set_intensity(uint32_t e, float v);
DSE_CAPI_GEN float dse_point_light_get_radius(uint32_t e);
DSE_CAPI_GEN void  dse_point_light_set_radius(uint32_t e, float v);
DSE_CAPI_GEN int  dse_point_light_get_enabled(uint32_t e);
DSE_CAPI_GEN void dse_point_light_set_enabled(uint32_t e, int v);

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

#ifdef __cplusplus
}
#endif

#endif // DSE_API_H not yet included
