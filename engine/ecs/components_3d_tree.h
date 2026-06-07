#ifndef DSE_COMPONENTS_3D_TREE_H
#define DSE_COMPONENTS_3D_TREE_H

#include <glm/glm.hpp>
#include <string>

namespace dse {

struct TreeComponent {
    bool enabled = true;

    // Mesh
    std::string mesh_path;                  ///< 树木模型路径
    std::string lod1_mesh_path;             ///< LOD1 简化模型（可选）
    std::string billboard_texture_path;     ///< 远距离 billboard 纹理（可选）

    // Distribution
    float density = 0.02f;                  ///< 每平方米树木数
    float spawn_radius = 120.0f;            ///< 相机周围生成半径
    unsigned int seed = 12345;
    float chunk_size = 32.0f;               ///< chunk 边长（米），比草地大

    // Variation
    float min_scale = 0.8f;
    float max_scale = 1.3f;
    float height_variation = 0.2f;          ///< ±高度随机比例
    bool random_rotation = true;            ///< 随机 Y 轴旋转

    // LOD
    float lod1_distance = 60.0f;            ///< LOD1 切换距离
    float billboard_distance = 150.0f;      ///< billboard 切换距离
    float cull_distance = 200.0f;           ///< 完全剔除距离

    // Wind (uses FoliageComponent shader path)
    float wind_strength = 0.3f;
    float wind_speed = 1.0f;

    // Shadow
    bool cast_shadow = true;
    float shadow_distance = 80.0f;

    // Runtime (managed by TreeSystem)
    int cached_instance_count_ = 0;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_TREE_H
