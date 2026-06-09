/**
 * @file dse_api.h
 * @brief DSEngine Native C ABI — Lua 与 C# 共享的底层引擎接口
 *
 * 纯 C 函数导出，消除 Lua / C# 两套绑定的重复逻辑。
 * C# 侧通过 Mono InternalCall 或 P/Invoke 调用。
 * Lua 侧 lua_binding_ecs_*.cpp 逐步迁移为调用本层函数。
 */

#ifndef DSE_API_H
#define DSE_API_H

#include <stdint.h>

#ifdef _WIN32
#  define DSE_CAPI __declspec(dllexport)
#else
#  define DSE_CAPI __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================
// Context Setup — 引擎初始化时调用一次
// ============================================================

DSE_CAPI void dse_native_api_init(
    void* world,           // World*
    void* asset_manager,   // AssetManager*
    void* audio_system,    // AudioSystem* (可为 nullptr)
    void  (*quit_fn)(void),
    void  (*set_title_fn)(const char*),
    float (*get_fps_fn)(void),
    void  (*set_fps_fn)(float),
    int   (*get_draw_calls_fn)(void)   // 可为 nullptr
);

// 供生成代码（dse_api.gen.cpp）访问内部 World 指针
DSE_CAPI void* dse_get_world_ptr(void);

// ============================================================
// Entity
// ============================================================

DSE_CAPI uint32_t dse_entity_create(void);
DSE_CAPI void     dse_entity_destroy(uint32_t e);
DSE_CAPI int      dse_entity_valid(uint32_t e);

// ============================================================
// TransformComponent
// ============================================================

DSE_CAPI void dse_transform_add(uint32_t e,
    float x, float y, float z,
    float sx, float sy, float sz);

DSE_CAPI void dse_transform_get_position(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_position(uint32_t e, float x, float y, float z);

DSE_CAPI void dse_transform_get_rotation(uint32_t e, float* x, float* y, float* z); // Euler degrees
DSE_CAPI void dse_transform_set_rotation(uint32_t e, float x, float y, float z);    // Euler degrees

DSE_CAPI void dse_transform_get_scale(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void dse_transform_set_scale(uint32_t e, float x, float y, float z);

// ============================================================
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
// TerrainTileManagerComponent
// ============================================================

DSE_CAPI int   dse_terrain_tile_get_enabled(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_enabled(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_tile_world_size(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_tile_world_size(uint32_t e, float v);
DSE_CAPI int   dse_terrain_tile_get_tile_resolution(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_tile_resolution(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_max_height(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_max_height(uint32_t e, float v);
DSE_CAPI float dse_terrain_tile_get_load_radius(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_load_radius(uint32_t e, float v);
DSE_CAPI float dse_terrain_tile_get_unload_radius(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_unload_radius(uint32_t e, float v);
DSE_CAPI int   dse_terrain_tile_get_use_procedural(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_use_procedural(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_procedural_base_height(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_procedural_base_height(uint32_t e, float v);
DSE_CAPI int   dse_terrain_tile_get_max_lod_levels(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_max_lod_levels(uint32_t e, int v);
DSE_CAPI float dse_terrain_tile_get_lod_distance_factor(uint32_t e);
DSE_CAPI void  dse_terrain_tile_set_lod_distance_factor(uint32_t e, float v);

// ============================================================
// DynamicObstacleComponent
// ============================================================

DSE_CAPI int   dse_dyn_obstacle_get_enabled(uint32_t e);
DSE_CAPI void  dse_dyn_obstacle_set_enabled(uint32_t e, int v);
DSE_CAPI int   dse_dyn_obstacle_get_shape(uint32_t e);   ///< 0=Box, 1=Cylinder
DSE_CAPI void  dse_dyn_obstacle_set_shape(uint32_t e, int v);
DSE_CAPI void  dse_dyn_obstacle_get_box_extents(uint32_t e, float* x, float* y, float* z);
DSE_CAPI void  dse_dyn_obstacle_set_box_extents(uint32_t e, float x, float y, float z);
DSE_CAPI float dse_dyn_obstacle_get_cylinder_radius(uint32_t e);
DSE_CAPI void  dse_dyn_obstacle_set_cylinder_radius(uint32_t e, float v);
DSE_CAPI float dse_dyn_obstacle_get_cylinder_height(uint32_t e);
DSE_CAPI void  dse_dyn_obstacle_set_cylinder_height(uint32_t e, float v);

// ============================================================
// NavMeshAutoRebakeComponent
// ============================================================

DSE_CAPI int   dse_navmesh_rebake_get_enabled(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_enabled(uint32_t e, int v);
DSE_CAPI float dse_navmesh_rebake_get_tile_size(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_tile_size(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_rebake_cooldown(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_rebake_cooldown(uint32_t e, float v);
DSE_CAPI int   dse_navmesh_rebake_get_collect_terrain(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_collect_terrain(uint32_t e, int v);
DSE_CAPI int   dse_navmesh_rebake_get_collect_mesh_renderers(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_collect_mesh_renderers(uint32_t e, int v);
DSE_CAPI float dse_navmesh_rebake_get_agent_height(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_height(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_agent_radius(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_radius(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_agent_max_climb(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_max_climb(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_agent_max_slope(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_agent_max_slope(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_cell_size(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_cell_size(uint32_t e, float v);
DSE_CAPI float dse_navmesh_rebake_get_cell_height(uint32_t e);
DSE_CAPI void  dse_navmesh_rebake_set_cell_height(uint32_t e, float v);

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

// MeshRenderer 材质/贴图加载（依赖 AssetManager）。
// set_material_from_dmat：从 .dmat 载入 MaterialInstance 并拷入 MeshRenderer，成功返回 1。
// set_texture：按 slot 名载入贴图并绑定到对应 handle，成功返回 1 并填充非空 out_*；slot 非法/加载失败返回 0。
DSE_CAPI int dse_mesh_renderer_set_material_from_dmat(uint32_t e, const char* dmat_path,
                                                      uint32_t material_index);
DSE_CAPI int dse_mesh_renderer_set_texture(uint32_t e, const char* slot, const char* path,
                                           uint32_t* out_handle, int* out_width, int* out_height);

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
DSE_CAPI void dse_ragdoll_set_collision_layer(uint32_t e, uint32_t layer, uint32_t mask);

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
DSE_CAPI void dse_buoyancy_set_water_level(uint32_t e, float water_level);
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
DSE_CAPI void dse_atmosphere_set_sun_intensity(uint32_t e, float r, float g, float b);

// DayNightCycle。set_location 中 day_of_year<=0=保持。get_sun_direction 填充 out_xyz(3)。
DSE_CAPI void dse_day_night_add(uint32_t e, float time_of_day, int auto_advance,
                                float time_speed);
DSE_CAPI void dse_day_night_set_time(uint32_t e, float time_of_day);
DSE_CAPI float dse_day_night_get_time(uint32_t e);
DSE_CAPI void dse_day_night_set_speed(uint32_t e, float speed);
DSE_CAPI void dse_day_night_set_auto_advance(uint32_t e, int enabled);
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
DSE_CAPI void dse_ik_set_weight(uint32_t e, int idx, float w);
DSE_CAPI void dse_ik_set_pole_vector(uint32_t e, int idx, float x, float y, float z);
DSE_CAPI void dse_ik_set_iterations(uint32_t e, int idx, int iters);
DSE_CAPI void dse_ik_set_enabled(uint32_t e, int enabled);

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

// ============================================================
// Input
// ============================================================

DSE_CAPI int   dse_input_get_key(int key_code);
DSE_CAPI int   dse_input_get_key_down(int key_code);
DSE_CAPI int   dse_input_get_key_up(int key_code);
DSE_CAPI int   dse_input_get_mouse_button(int button);
DSE_CAPI int   dse_input_get_mouse_button_down(int button);
DSE_CAPI int   dse_input_get_mouse_button_up(int button);
DSE_CAPI float dse_input_get_mouse_x(void);
DSE_CAPI float dse_input_get_mouse_y(void);
DSE_CAPI float dse_input_get_mouse_scroll(void);
DSE_CAPI float dse_input_get_gamepad_axis(int gamepad_id, int axis);

// ============================================================
// Assets
// ============================================================

DSE_CAPI uint32_t dse_assets_load_texture(const char* path);
DSE_CAPI void     dse_assets_set_data_root(const char* path);

// ============================================================
// App / System
// ============================================================

DSE_CAPI void  dse_app_quit(void);
DSE_CAPI void  dse_app_set_window_title(const char* title);
DSE_CAPI float dse_app_get_time(void);
DSE_CAPI float dse_app_get_delta_time(void);
DSE_CAPI void  dse_app_set_target_fps(float fps);
DSE_CAPI float dse_app_get_target_fps(void);

// ============================================================
// Metrics
// ============================================================

DSE_CAPI int dse_metrics_get_draw_calls(void);

#ifdef __cplusplus
}
#endif

#endif // DSE_API_H
