#ifndef DSE_RAGDOLL_SYSTEM_H
#define DSE_RAGDOLL_SYSTEM_H

#include "engine/ecs/world.h"

class AssetManager;

namespace dse {
struct RagdollComponent;

namespace physics3d { class Physics3DSystem; }

namespace gameplay3d {

/**
 * @class RagdollSystem
 * @brief 布娃娃物理系统
 *
 * 工作流程：
 *   1. 检测 RagdollComponent::active 标志
 *   2. 激活时：从 Animator3DComponent 骨骼层级自动生成刚体+关节
 *   3. 每帧：将 PhysX 刚体位姿写回 Animator3DComponent::final_bone_matrices
 *   4. 停用时：销毁所有物理对象，恢复动画驱动
 */
class RagdollSystem {
public:
    RagdollSystem() = default;
    ~RagdollSystem() = default;

    void SetAssetManager(AssetManager* asset_manager);
    void SetPhysics3D(physics3d::Physics3DSystem* physics3d);

    void FixedUpdate(World& world, float dt);

    /// 激活指定实体的布娃娃，可选施加初始冲量
    void Activate(World& world, entt::entity entity,
                  const glm::vec3& impulse = glm::vec3(0.0f),
                  const glm::vec3& impulse_point = glm::vec3(0.0f));

    /// 停用布娃娃，回到动画驱动
    void Deactivate(World& world, entt::entity entity);

private:
    AssetManager* asset_manager_ = nullptr;
    physics3d::Physics3DSystem* physics3d_ = nullptr;

    void AutoSetupBones(World& world, entt::entity entity, RagdollComponent& ragdoll);
    void CreatePhysicsBodies(World& world, entt::entity entity, RagdollComponent& ragdoll);
    void DestroyPhysicsBodies(RagdollComponent& ragdoll);
    void DestroyPhysicsBodiesJolt(World& world, RagdollComponent& ragdoll);
    void SyncBonesFromPhysics(World& world, entt::entity entity, RagdollComponent& ragdoll);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_RAGDOLL_SYSTEM_H
