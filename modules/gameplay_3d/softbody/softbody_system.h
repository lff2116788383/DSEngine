#ifndef DSE_SOFTBODY_SYSTEM_H
#define DSE_SOFTBODY_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
struct SoftBodyComponent;

namespace gameplay3d {

/**
 * @class SoftBodySystem
 * @brief PBD（Position Based Dynamics）软体模拟系统
 *
 * 每帧模拟流程：
 *   1. 从 mesh 顶点初始化粒子和距离约束
 *   2. 施加外力（重力）→ 预测位置
 *   3. 迭代求解距离约束 + 体积保持约束
 *   4. 更新速度，施加阻尼
 *   5. 回写 mesh 顶点，标记 mesh_dirty
 */
class SoftBodySystem {
public:
    SoftBodySystem() = default;
    ~SoftBodySystem() = default;

    void SetAssetManager(AssetManager* asset_manager);

    void FixedUpdate(World& world, float dt);

private:
    AssetManager* asset_manager_ = nullptr;

    void InitializeFromMesh(World& world, entt::entity entity, SoftBodyComponent& sb);
    void Simulate(SoftBodyComponent& sb, float dt);
    void ProjectDistanceConstraints(SoftBodyComponent& sb);
    void ProjectVolumeConstraint(SoftBodyComponent& sb);
    void ProjectCollisions(World& world, SoftBodyComponent& sb);
    void WriteBackMesh(World& world, entt::entity entity, SoftBodyComponent& sb);
    float ComputeVolume(const SoftBodyComponent& sb, const std::vector<unsigned short>& indices) const;
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_SOFTBODY_SYSTEM_H
