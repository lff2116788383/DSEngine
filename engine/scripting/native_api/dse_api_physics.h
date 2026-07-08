/**
 * @file dse_api_physics.h
 * @brief DSEngine Native C ABI — Physics (3D/2D/Colliders/Joints) module
 *
 * Split from dse_api.h for maintainability. Include dse_api.h for all modules.
 */

#ifndef DSE_API_PHYSICS_H
#define DSE_API_PHYSICS_H

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



#ifdef __cplusplus
}
#endif

#endif // DSE_API_PHYSICS_H
