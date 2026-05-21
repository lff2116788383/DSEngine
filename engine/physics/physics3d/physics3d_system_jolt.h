#ifndef DSE_PHYSICS3D_SYSTEM_JOLT_H
#define DSE_PHYSICS3D_SYSTEM_JOLT_H

#ifdef DSE_ENABLE_JOLT

#include "engine/physics/physics3d/i_physics3d_system.h"
#include "engine/ecs/components_3d_physics.h"
#include <memory>
#include <unordered_map>
#include <string>
#include <glm/glm.hpp>

// Jolt 前向声明
namespace JPH {
    class Shape;
}

namespace dse {
namespace physics3d {

class Physics3DSystem : public IPhysics3DSystem {
public:
    Physics3DSystem();
    ~Physics3DSystem() override;

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

    // 角色控制器 API
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

    // PhysX 局永桥（Jolt 模式返回 nullptr）
    void* GetPxPhysics() const override { return nullptr; }
    void* GetPxScene() const override { return nullptr; }
    void* GetPxCooking() const override { return nullptr; }

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;

    World* world_cache_ = nullptr;

    std::vector<CollisionEvent> collision_events_;
    std::vector<TriggerEvent> trigger_events_;

    void SyncTransformsToPhysics(World& world);
    void SyncPhysicsToTransforms(World& world);
    void SyncCharacterControllers(World& world, float fixed_delta_time);
    void CreateCharacterActor(World& world, entt::entity entity, CharacterController3DComponent& cc, const ::TransformComponent& transform);
    void SyncJoints(World& world);
    void CheckBrokenJoints(World& world);

    // MeshCollider 辅助函数
    JPH::Shape* CreateConvexHullShape(const std::vector<glm::vec3>& vertices, const std::string& mesh_path);
    JPH::Shape* CreateTriangleMeshShape(const std::vector<glm::vec3>& vertices, const std::vector<uint32_t>& indices, const std::string& mesh_path);
};

} // namespace physics3d
} // namespace dse

#endif // DSE_ENABLE_JOLT
#endif // DSE_PHYSICS3D_SYSTEM_JOLT_H
