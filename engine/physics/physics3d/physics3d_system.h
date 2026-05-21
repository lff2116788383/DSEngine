#ifndef DSE_PHYSICS3D_SYSTEM_H
#define DSE_PHYSICS3D_SYSTEM_H

#include "engine/physics/physics3d/i_physics3d_system.h"
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <tuple>
#include <cfloat>

namespace physx {
    class PxFoundation;
    class PxPhysics;
    class PxScene;
    class PxCpuDispatcher;
    class PxMaterial;
    class PxConvexMesh;
    class PxTriangleMesh;
    class PxJoint;
    class PxCooking;
}

namespace dse {

struct CharacterController3DComponent;
struct CapsuleCollider3DComponent;
struct Joint3DComponent;

namespace physics3d {

class Physics3DSystem : public IPhysics3DSystem {
public:
    Physics3DSystem() = default;
    ~Physics3DSystem() override = default;

    bool Init(World& world) override;
    void Shutdown() override;
    void FixedUpdate(World& world, float fixed_delta_time) override;

    // 射线检测
    RaycastResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance) override;

    // 刚体动力学 API
    void AddForce(entt::entity entity, const glm::vec3& force) override;
    void AddTorque(entt::entity entity, const glm::vec3& torque) override;
    void AddImpulse(entt::entity entity, const glm::vec3& impulse) override;
    void SetVelocity(entt::entity entity, const glm::vec3& velocity) override;
    void SetAngularVelocity(entt::entity entity, const glm::vec3& angular_velocity) override;
    glm::vec3 GetVelocity(entt::entity entity) const override;
    glm::vec3 GetAngularVelocity(entt::entity entity) const override;
    void SetGravityEnabled(entt::entity entity, bool enabled) override;
    bool IsGravityEnabled(entt::entity entity) const override;
    void RemoveActor(entt::entity entity) override;

    // 角色控制器 API（基于 PxScene::sweep 的自定义实现，不依赖 PxControllerManager）
    CharacterMoveResult MoveCharacter(entt::entity entity, const glm::vec3& displacement, float min_dist, float delta_time) override;
    bool JumpCharacter(entt::entity entity, float jump_speed) override;
    bool IsCharacterGrounded(entt::entity entity) const override;
    glm::vec3 GetCharacterPosition(entt::entity entity) const override;

    // 碰撞/触发事件查询 API
    const std::vector<CollisionEvent>& GetCollisionEvents() const override { return collision_events_; }
    const std::vector<TriggerEvent>& GetTriggerEvents() const override { return trigger_events_; }
    void FlushEvents() override { collision_events_.clear(); trigger_events_.clear(); }

    // 碰撞层设置 API
    void SetCollisionLayer(entt::entity entity, uint16_t layer, uint16_t mask) override;

    // PhysX 底层指针访问
    void* GetPxPhysics() const override { return physics_; }
    void* GetPxScene() const override { return scene_; }
    void* GetPxCooking() const override { return cooking_; }

private:
    physx::PxFoundation* foundation_ = nullptr;
    physx::PxPhysics* physics_ = nullptr;
    physx::PxScene* scene_ = nullptr;
    physx::PxCpuDispatcher* dispatcher_ = nullptr;
    physx::PxMaterial* default_material_ = nullptr;
    physx::PxCooking* cooking_ = nullptr;
    World* world_cache_ = nullptr; ///< Init 时缓存的 World 指针，供动力学 API 使用

    // 碰撞/触发事件队列（Task 1）
    std::vector<CollisionEvent> collision_events_;
    std::vector<TriggerEvent> trigger_events_;
    void* simulation_callback_ = nullptr; ///< DseSimulationEventCallback*

    // 材质缓存（Task 2）— key = (static_friction, dynamic_friction, restitution)
    struct MaterialKey {
        float sf, df, rest;
        bool operator<(const MaterialKey& o) const {
            if (sf != o.sf) return sf < o.sf;
            if (df != o.df) return df < o.df;
            return rest < o.rest;
        }
    };
    std::map<MaterialKey, physx::PxMaterial*> material_cache_;
    physx::PxMaterial* GetOrCreateMaterial(float friction, float bounciness);

    // Mesh 碰撞体缓存（Task 3）
    std::unordered_map<std::string, physx::PxConvexMesh*> convex_mesh_cache_;
    std::unordered_map<std::string, physx::PxTriangleMesh*> triangle_mesh_cache_;

    // Joint 追踪（Task 5）
    void SyncJoints(World& world);
    void CheckBrokenJoints(World& world);

    void SyncTransformsToPhysics(World& world);
    void SyncPhysicsToTransforms(World& world);
    void SyncCharacterControllers(World& world, float fixed_delta_time);
    void CreateCharacterActor(World& world, entt::entity entity, CharacterController3DComponent& cc, const ::TransformComponent& transform);
};

} // namespace physics3d
} // namespace dse

#endif // DSE_PHYSICS3D_SYSTEM_H