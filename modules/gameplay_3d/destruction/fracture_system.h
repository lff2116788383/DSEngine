#ifndef DSE_FRACTURE_SYSTEM_H
#define DSE_FRACTURE_SYSTEM_H

#include "engine/ecs/world.h"
#include <string>
#include <memory>
#include <unordered_map>

class AssetManager;

namespace dse {
struct FractureAsset;

namespace physics3d {
class IPhysics3DSystem;
}
} // namespace dse

namespace dse {
namespace gameplay3d {

/**
 * @class FractureSystem
 * @brief 运行时破坏系统 —— 监控 FractureComponent 的断裂条件，生成碎片实体并管理其生命周期。
 *
 * 支持两种碎片来源：
 *   - Prefractured：离线预切分（从 fracture.json 加载碎片 dmesh）
 *   - RuntimeVoronoi：碰撞时实时计算 Voronoi 切分，结果缓存
 *
 * 工作流程：
 *   1. Update() 检查每个 FractureComponent 的触发条件
 *   2. 触发时加载/生成碎片描述
 *   3. 隐藏原始 mesh，生成碎片实体（Mesh + RigidBody + Collider + Tag）
 *   4. 对每个碎片施加径向爆炸力
 *   5. 跟踪碎片生命周期，淡出后销毁
 */
class FractureSystem {
public:
    FractureSystem() = default;
    ~FractureSystem() = default;

    void SetAssetManager(AssetManager* asset_manager);
    void SetPhysics3D(physics3d::IPhysics3DSystem* physics3d);

    /// 每帧更新：检查触发条件 + 管理碎片生命周期
    void Update(World& world, float delta_time);

    // --- 手动 API（可从 Lua/C++ 调用）---

    /// 对可破坏实体施加伤害
    void ApplyDamage(World& world, entt::entity entity, float damage,
                     const glm::vec3& impact_point = glm::vec3(0.0f),
                     const glm::vec3& impact_dir = glm::vec3(0.0f, 1.0f, 0.0f));

    /// 立即触发破碎（跳过生命值/力检查）
    void TriggerFracture(World& world, entt::entity entity,
                         const glm::vec3& impact_point = glm::vec3(0.0f),
                         const glm::vec3& impact_dir = glm::vec3(0.0f, 1.0f, 0.0f));

private:
    AssetManager* asset_manager_ = nullptr;
    physics3d::IPhysics3DSystem* physics3d_ = nullptr;

    /// 加载或获取缓存的破碎资产（从 JSON 描述文件）
    std::shared_ptr<FractureAsset> LoadFractureAsset(const std::string& path);

    /// 运行时 Voronoi 切分：从实体的 mesh 数据实时计算碎片
    std::shared_ptr<FractureAsset> ComputeRuntimeVoronoi(
        World& world, entt::entity entity,
        uint32_t fragment_count, uint32_t seed,
        bool cluster_near_impact, const glm::vec3& impact_point);

    /// 生成碎片实体
    void SpawnFragments(World& world, entt::entity source_entity);

    /// 每帧碎片生命周期：淡出 + 销毁过期碎片
    void UpdateFragmentLifecycle(World& world, float delta_time);

    /// 已加载的破碎资产缓存
    std::unordered_map<std::string, std::shared_ptr<FractureAsset>> fracture_asset_cache_;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_FRACTURE_SYSTEM_H
