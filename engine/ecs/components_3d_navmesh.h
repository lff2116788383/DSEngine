#ifndef DSE_COMPONENTS_3D_NAVMESH_H
#define DSE_COMPONENTS_3D_NAVMESH_H

#include <glm/glm.hpp>
#include <string>

namespace dse {

/// Dynamic obstacle component - entities with this cause NavMesh local rebake
struct DynamicObstacleComponent {
    bool enabled = true;

    enum class Shape { Box, Cylinder };
    Shape shape = Shape::Box;

    // Box extents (half-sizes)
    glm::vec3 box_extents = glm::vec3(1.0f, 2.0f, 1.0f);

    // Cylinder parameters
    float cylinder_radius = 1.0f;
    float cylinder_height = 2.0f;

    // Runtime state (managed by NavMeshSystem)
    unsigned int obstacle_ref_ = 0;  ///< dtTileCache obstacle reference
    bool dirty_ = true;              ///< needs add/update in tile cache
};

/// NavMesh auto-rebake configuration component
struct NavMeshAutoRebakeComponent {
    bool enabled = true;

    // Tile size for tiled navmesh (world units)
    float tile_size = 48.0f;

    // Rebake trigger
    float rebake_cooldown = 1.0f;        ///< minimum seconds between rebakes
    bool collect_terrain = true;          ///< include TerrainComponent geometry
    bool collect_mesh_renderers = true;   ///< include MeshRendererComponent geometry

    // Build config override (agent params)
    float agent_height = 2.0f;
    float agent_radius = 0.6f;
    float agent_max_climb = 0.9f;
    float agent_max_slope = 45.0f;
    float cell_size = 0.3f;
    float cell_height = 0.2f;

    // Runtime state
    float cooldown_timer_ = 0.0f;
    bool needs_full_rebake_ = true;
    int baked_tile_count_ = 0;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_NAVMESH_H
