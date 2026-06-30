/**
 * @file hlod_system.h
 * @brief Hierarchical Level of Detail (HLOD) 系统
 *
 * 将空间中相邻的多个小 mesh 组合成简化的代理 mesh（proxy），当相机远离时
 * 用一个 proxy 替代多个原始物体的渲染，大幅减少 draw call 和顶点数。
 *
 * 层级结构：
 * - Level 0: 原始物体（最高细节）
 * - Level 1: 小范围簇合并的 proxy
 * - Level 2: 更大范围簇进一步合并
 * - ...
 *
 * 运行时行为：
 * - 每帧根据相机距离决定每个 HLODCluster 使用哪一级
 * - 切换时隐藏子物体、显示 proxy（或反之）
 * - 与 LODGroupComponent 协同：HLOD 在 LODGroup 最远级之后接管
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include "engine/core/dse_export.h"

using Entity = entt::entity;
class World;

namespace dse {
namespace render {

// ─── 离线构建数据 ──────────────────────────────────────────────────────────

/// HLOD 层级中的代理 mesh 信息
struct HLODProxy {
    std::string mesh_path;            ///< 已合并简化的代理 mesh 路径
    std::string material_path;        ///< 代理材质路径（atlas 或简化材质）
    float switch_distance = 0.0f;     ///< 从此距离开始使用此 proxy
    uint32_t triangle_count = 0;      ///< 代理三角形数
    glm::vec3 bounds_center{0.0f};
    glm::vec3 bounds_extents{0.0f};
};

/// HLOD 簇：一组相邻物体共享的多级代理层次
struct HLODCluster {
    std::string name;
    std::vector<Entity> source_entities;    ///< Level 0 的原始实体列表
    std::vector<HLODProxy> levels;          ///< 代理层级（按距离升序）
    glm::vec3 bounds_center{0.0f};
    glm::vec3 bounds_extents{0.0f};
    int active_level = -1;                  ///< -1 = 使用原始物体，>=0 = 使用 proxy[active_level]
    Entity proxy_entity = Entity{entt::null}; ///< 运行时代理渲染实体
};

// ─── ECS 组件 ───────────────────────────────────────────────────────────────

/// 标记实体属于某个 HLOD 簇
struct HLODMemberComponent {
    uint32_t cluster_index = 0;   ///< 所属 HLODCluster 的索引
    bool hidden_by_hlod = false;  ///< 当前是否被 HLOD proxy 替代而隐藏
};

/// HLOD 配置组件，附加到场景根
struct HLODConfigComponent {
    bool enabled = true;
    std::string hlod_data_path;       ///< HLOD 数据文件路径（.dhlod）
    float distance_scale = 1.0f;      ///< 全局距离缩放因子
    float hysteresis = 0.1f;          ///< 切换死区比例
};

// ─── 离线构建 ──────────────────────────────────────────────────────────────

/// HLOD 构建配置
struct HLODBuildConfig {
    float cluster_radius = 64.0f;       ///< 簇划分半径
    uint32_t max_cluster_triangles = 100000; ///< 每个簇最大三角形数
    uint32_t hlod_levels = 2;           ///< 代理层级数
    float simplify_ratio = 0.25f;       ///< 每级简化比率
    float level_distance_multiplier = 2.0f; ///< 层级间距离倍数
    float base_distance = 100.0f;       ///< 第一级切换距离
};

/// 构建输入中的 mesh 信息
struct HLODBuildMesh {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<uint32_t> indices;
    glm::mat4 transform{1.0f};
    uint32_t entity_index = 0;
};

/// 构建结果
struct HLODBuildResult {
    bool success = false;
    std::vector<HLODCluster> clusters;
    std::string error_message;
};

/// HLOD 离线构建器
class DSE_EXPORT HLODBuilder {
public:
    /// 根据输入 mesh 列表构建 HLOD 层级
    static HLODBuildResult Build(const std::vector<HLODBuildMesh>& meshes,
                                 const HLODBuildConfig& config);

    /// 序列化 HLOD 数据到文件
    static bool SaveToFile(const std::vector<HLODCluster>& clusters,
                           const std::string& output_path);

    /// 从文件加载 HLOD 数据
    static bool LoadFromFile(const std::string& path,
                             std::vector<HLODCluster>& out_clusters);
};

// ─── 运行时系统 ─────────────────────────────────────────────────────────────

class DSE_EXPORT HLODSystem {
public:
    HLODSystem() = default;
    ~HLODSystem() = default;

    /// 初始化：加载 HLOD 数据，创建 proxy 实体
    bool Init(::World& world, const std::string& hlod_data_path);

    /// 每帧更新：根据相机距离决定 HLOD 级别
    void Update(::World& world, const glm::vec3& camera_pos);

    /// 关闭：清除所有 proxy 实体
    void Shutdown(::World& world);

    /// 获取当前加载的簇列表
    const std::vector<HLODCluster>& GetClusters() const { return clusters_; }

    /// 统计当前活跃 proxy 数量
    size_t ActiveProxyCount() const;

private:
    void SwitchClusterLevel(::World& world, HLODCluster& cluster, int new_level);

    std::vector<HLODCluster> clusters_;
    float distance_scale_ = 1.0f;
    float hysteresis_ = 0.1f;
    bool initialized_ = false;
};

} // namespace render
} // namespace dse
