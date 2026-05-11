#ifndef DSE_COMPONENTS_3D_CLOTH_H
#define DSE_COMPONENTS_3D_CLOTH_H

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace dse {

/// 两个粒子之间的距离约束
struct ClothDistanceConstraint {
    uint32_t i = 0;          ///< 粒子索引 A
    uint32_t j = 0;          ///< 粒子索引 B
    float rest_length = 0.0f; ///< A 与 B 之间的静止距离
};

/// 弯曲约束（跨越共享边的两个相邻三角形）
struct ClothBendConstraint {
    uint32_t i0 = 0;  ///< 三角形 1 的对顶点
    uint32_t i1 = 0;  ///< 共享边顶点 A
    uint32_t i2 = 0;  ///< 共享边顶点 B
    uint32_t i3 = 0;  ///< 三角形 2 的对顶点
    float rest_angle = 0.0f; ///< 静止二面角（弧度）
};

/// 布料碰撞用球体碰撞体
struct ClothSphereCollider {
    uint32_t entity_id = UINT32_MAX;    ///< 带 Transform 的实体 ID（position = 球心）
    float radius = 0.5f;
};

/// 布料碰撞用胶囊碰撞体
struct ClothCapsuleCollider {
    uint32_t entity_id = UINT32_MAX;    ///< 带 Transform 的实体 ID
    float radius = 0.3f;
    float half_height = 0.5f;          ///< 沿局部 Y 轴的半高度
};

/// 布料模拟 ECS 组件
struct ClothComponent {
    // --- 配置（可序列化）---
    bool enabled = true;
    std::string source_mesh_path;          ///< 可选：用于初始化布料的 mesh 路径

    // 求解器参数
    uint32_t solver_iterations = 8;        ///< 每步约束投影迭代次数
    float damping = 0.01f;                 ///< 速度阻尼系数 (0~1)
    float stiffness = 1.0f;               ///< 距离约束刚度 (0=刚性, 1=柔软)
    float bend_stiffness = 0.5f;           ///< 弯曲约束刚度
    float friction = 0.3f;                 ///< 碰撞摩擦系数

    // 外力
    glm::vec3 gravity = {0.0f, -9.81f, 0.0f};
    glm::vec3 wind = {0.0f, 0.0f, 0.0f};
    float wind_turbulence = 0.0f;          ///< 随机风力扰动振幅

    // 碰撞
    float collision_radius = 0.02f;        ///< 每个粒子的碰撞球半径
    std::vector<ClothSphereCollider> sphere_colliders;
    std::vector<ClothCapsuleCollider> capsule_colliders;

    // 固定顶点（不可移动的粒子，跟随 mesh/骨骼变换）
    std::vector<uint32_t> pinned_vertices;

    // --- 运行时状态（不序列化）---
    bool initialized = false;
    uint32_t particle_count = 0;

    std::vector<glm::vec3> positions;      ///< 当前粒子位置（世界空间）
    std::vector<glm::vec3> prev_positions; ///< 上一帧位置（用于 Verlet 积分）
    std::vector<glm::vec3> velocities;     ///< 粒子速度
    std::vector<float> inv_masses;         ///< 逆质量（0 = 固定点）
    std::vector<glm::vec3> rest_positions; ///< 原始 mesh 顶点位置（局部空间）

    // 约束（初始化时构建）
    std::vector<ClothDistanceConstraint> distance_constraints;
    std::vector<ClothBendConstraint> bend_constraints;

    // 输出用 mesh 拓扑
    std::vector<uint32_t> triangle_indices; ///< 原始三角形索引
    std::vector<glm::vec3> normals;         ///< 计算后的逐顶点法线
    std::vector<glm::vec2> uvs;             ///< 原始 UV（不变）

    // GPU mesh 更新
    bool mesh_dirty = false;                ///< 通知渲染器重新上传顶点数据
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_CLOTH_H
