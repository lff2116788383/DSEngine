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

    // Runtime state（预留字段：dtTileCache obstacle 引用）
    // TODO: [2026-08-14] 当前无运行时系统消费本组件（dtTileCache 未启用）——
    // 仅编辑器增删/序列化/反射/复制层可用，动态障碍刻入 NavMesh 的行为尚未实现。
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

    // Runtime state（预留字段：自动重烘焙调度状态）
    // TODO: [2026-08-14] 当前无运行时系统消费本组件——自动重烘焙行为尚未实现，
    // 字段仅供编辑器配置/序列化往返；接入需启用 dtTileCache + 重烘焙调度。
    float cooldown_timer_ = 0.0f;
    bool needs_full_rebake_ = true;
    int baked_tile_count_ = 0;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_NAVMESH_H
