#ifndef DSE_PHYSICS3D_SYSTEM_JOLT_H
#define DSE_PHYSICS3D_SYSTEM_JOLT_H

#ifdef DSE_ENABLE_JOLT

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_physics.h"
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <map>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace dse {
namespace physics3d {

struct CollisionEvent {
    enum class Type { Enter, Stay, Exit };
    Type type = Type::Enter;
    entt::entity entity_a = entt::null;
    entt::entity entity_b = entt::null;
    glm::vec3 contact_point = glm::vec3(0.0f);
    glm::vec3 contact_normal = glm::vec3(0.0f);
    float impulse = 0.0f;
};

struct TriggerEvent {
    enum class Type { Enter, Exit };
    Type type = Type::Enter;
    entt::entity trigger_entity = entt::null;
    entt::entity other_entity = entt::null;
};

struct RaycastResult {
    bool hit = false;
    entt::entity entity = entt::null;
    glm::vec3 hit_point = glm::vec3(0.0f);
    glm::vec3 hit_normal = glm::vec3(0.0f);
    float distance = 0.0f;
};

struct CharacterMoveResult {
    bool is_grounded = false;
    glm::vec3 velocity = glm::vec3(0.0f);
    uint8_t collision_flags = 0;
};

class Physics3DSystem {
public:
    Physics3DSystem();
    ~Physics3DSystem();

    bool Init(World& world);
    void Shutdown();
    void FixedUpdate(World& world, float fixed_delta_time);

    // 射线检测
    RaycastResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance);

    // 刚体动力学 API
    void AddForce(entt::entity entity, const glm::vec3& force);
    void AddImpulse(entt::entity entity, const glm::vec3& impulse);
    void SetVelocity(entt::entity entity, const glm::vec3& velocity);
    glm::vec3 GetVelocity(entt::entity entity) const;
    void SetGravityEnabled(entt::entity entity, bool enabled);
    bool IsGravityEnabled(entt::entity entity) const;
    void RemoveActor(entt::entity entity);

    // 角色控制器 API
    CharacterMoveResult MoveCharacter(entt::entity entity, const glm::vec3& displacement, float min_dist, float delta_time);
    bool JumpCharacter(entt::entity entity, float jump_speed);
    bool IsCharacterGrounded(entt::entity entity) const;
    glm::vec3 GetCharacterPosition(entt::entity entity) const;

    // 碰撞/触发事件查询 API
    const std::vector<CollisionEvent>& GetCollisionEvents() const { return collision_events_; }
    const std::vector<TriggerEvent>& GetTriggerEvents() const { return trigger_events_; }

    // 碰撞层设置 API
    void SetCollisionLayer(entt::entity entity, uint16_t layer, uint16_t mask);

    // PhysX 兼容桩（Ragdoll 等系统在 Jolt 模式下使用 ECS 公共 API）
    void* GetPxPhysics() const { return nullptr; }
    void* GetPxScene() const { return nullptr; }
    void* GetPxCooking() const { return nullptr; }

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
};

} // namespace physics3d
} // namespace dse

#endif // DSE_ENABLE_JOLT
#endif // DSE_PHYSICS3D_SYSTEM_JOLT_H
