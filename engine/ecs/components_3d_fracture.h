#ifndef DSE_COMPONENTS_3D_FRACTURE_H
#define DSE_COMPONENTS_3D_FRACTURE_H

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <cstdint>

namespace dse {

/// 单个碎片描述（支持离线预切分和运行时 Voronoi）
struct FragmentDescriptor {
    std::string mesh_path;               ///< 碎片 .dmesh 文件路径（预切分模式）
    glm::vec3 local_offset = glm::vec3(0.0f); ///< 碎片质心相对于原始 mesh 原点的偏移
    float volume = 1.0f;                 ///< 相对体积（用于质量分配）

    // 运行时 Voronoi 碎片的内存数据（mesh_path 为缓存键时使用）
    std::vector<float> runtime_vertices;          ///< 碎片顶点数据（质心居中）
    std::vector<uint32_t> runtime_indices;  ///< 碎片三角形索引
    int runtime_vertex_stride = 0;                ///< 顶点步长（float 个数，0=使用 dmesh 加载）
};

/// 从 JSON 加载的破碎资产描述
struct FractureAsset {
    std::string source_mesh;             ///< 原始（完整）mesh 路径
    std::vector<FragmentDescriptor> fragments;
};

/// 破碎触发模式
enum class FractureTriggerMode {
    ImpactForce = 0,   ///< 单次冲击：冲击力超过 break_force 时立即碎裂
    DamageAccumulation = 1  ///< 累积伤害：生命值降至 0 时碎裂
};

/// 碎片来源模式
enum class FractureSource {
    Prefractured = 0,      ///< 离线预切分（从 fracture_asset_path 加载碎片 JSON）
    RuntimeVoronoi = 1     ///< 运行时按需 Voronoi 切分（碰撞时实时计算，结果缓存）
};

/// 可破坏实体的 ECS 组件
struct FractureComponent {
    // --- 配置（可序列化）---
    FractureSource source = FractureSource::Prefractured;
    std::string fracture_asset_path;     ///< 破碎描述 JSON 路径（Prefractured 模式）
    FractureTriggerMode trigger_mode = FractureTriggerMode::ImpactForce;

    // 运行时 Voronoi 配置（RuntimeVoronoi 模式）
    uint32_t runtime_fragment_count = 8; ///< 实时切分碎片数量
    uint32_t runtime_seed = 0;           ///< Voronoi 种子（0=基于冲击点自动生成）
    bool cluster_near_impact = true;     ///< 种子点集中在冲击点附近（更真实的碎裂分布）
    float break_force = 1000.0f;         ///< 冲击力阈值（ImpactForce 模式）
    float health = 100.0f;               ///< 当前生命值（DamageAccumulation 模式）
    float max_health = 100.0f;           ///< 最大生命值

    // 碎片行为
    float fragment_lifetime = 5.0f;      ///< 碎片开始淡出前的存活时间（秒）
    float fragment_fade_duration = 1.0f; ///< 淡出持续时间（秒）
    float explosion_force = 50.0f;       ///< 碎裂时施加的径向爆炸力
    float fragment_mass_scale = 1.0f;    ///< 碎片质量倍率

    // 材质继承
    bool inherit_material = true;        ///< 碎片是否继承父物体材质设置
    std::string fragment_shader_variant = "MESH_LIT";

    // --- 运行时状态（不序列化）---
    bool is_fractured = false;           ///< 是否已碎裂
    bool fracture_requested = false;     ///< 是否有待处理的碎裂请求
    glm::vec3 impact_point = glm::vec3(0.0f);  ///< 碎裂冲击点位置
    glm::vec3 impact_direction = glm::vec3(0.0f, 1.0f, 0.0f); ///< 冲击方向

    // 缓存的破碎资产（首次使用时加载）
    std::shared_ptr<FractureAsset> cached_asset;
};

/// 轻量标签组件，附加到生成的碎片实体上
struct FragmentTagComponent {
    uint32_t source_entity_id = UINT32_MAX; ///< 产生该碎片的原始实体 ID
    float elapsed = 0.0f;                ///< 碎片已存活时间（秒）
    float lifetime = 5.0f;               ///< 开始淡出前的总存活时间
    float fade_duration = 1.0f;          ///< 淡出持续时间
    float initial_alpha = 1.0f;          ///< 初始透明度（用于淡出计算）

    // CPU 回退物理（PhysX 未启用时使用）
    glm::vec3 velocity = glm::vec3(0.0f);        ///< 线速度
    glm::vec3 angular_velocity = glm::vec3(0.0f); ///< 角速度（弧度/秒）
    bool cpu_physics = false;             ///< 是否使用 CPU 回退物理
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_FRACTURE_H
