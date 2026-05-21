/**
 * @file i_physics3d_system.h
 * @brief 3D 物理后端统一抽象接口
 *
 * 所有调用方通过 IPhysics3DSystem* 访问物理功能，
 * 与具体后端（PhysX / Jolt）完全解耦。
 * 工厂选择逻辑仅在 frame_pipeline.cpp 中的一处 #if 块内存在。
 */

#pragma once

#include "engine/ecs/world.h"
#include <glm/glm.hpp>
#include <entt/entt.hpp>
#include <vector>
#include <cstdint>
#include <memory>

#if defined(DSE_ENABLE_PHYSX) || defined(DSE_ENABLE_JOLT)
#define DSE_HAS_PHYSICS3D 1
#endif

namespace dse {
namespace physics3d {

// ---------------------------------------------------------------------------
// 共享数据结构（从两个后端实现中提取，消除重复定义）
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// IPhysics3DSystem — 纯虚接口
// ---------------------------------------------------------------------------

class IPhysics3DSystem {
public:
    virtual ~IPhysics3DSystem() = default;

    virtual bool Init(World& world) = 0;
    virtual void Shutdown() = 0;
    virtual void FixedUpdate(World& world, float fixed_delta_time) = 0;

    // 射线检测
    virtual RaycastResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance) = 0;

    // 刚体动力学
    virtual void AddForce(entt::entity entity, const glm::vec3& force) = 0;
    virtual void AddTorque(entt::entity entity, const glm::vec3& torque) = 0;
    virtual void AddImpulse(entt::entity entity, const glm::vec3& impulse) = 0;
    virtual void SetVelocity(entt::entity entity, const glm::vec3& velocity) = 0;
    virtual void SetAngularVelocity(entt::entity entity, const glm::vec3& angular_velocity) = 0;
    virtual glm::vec3 GetVelocity(entt::entity entity) const = 0;
    virtual glm::vec3 GetAngularVelocity(entt::entity entity) const = 0;
    virtual void SetGravityEnabled(entt::entity entity, bool enabled) = 0;
    virtual bool IsGravityEnabled(entt::entity entity) const = 0;
    virtual void RemoveActor(entt::entity entity) = 0;

    // 角色控制器
    virtual CharacterMoveResult MoveCharacter(entt::entity entity, const glm::vec3& displacement,
                                               float min_dist, float delta_time) = 0;
    virtual bool JumpCharacter(entt::entity entity, float jump_speed) = 0;
    virtual bool IsCharacterGrounded(entt::entity entity) const = 0;
    virtual glm::vec3 GetCharacterPosition(entt::entity entity) const = 0;

    // 碰撞/触发事件
    virtual const std::vector<CollisionEvent>& GetCollisionEvents() const = 0;
    virtual const std::vector<TriggerEvent>& GetTriggerEvents() const = 0;
    virtual void FlushEvents() = 0;

    // 碰撞层
    virtual void SetCollisionLayer(entt::entity entity, uint16_t layer, uint16_t mask) = 0;

    // PhysX 原始指针访问（Jolt 后端返回 nullptr，供 ragdoll 等 PhysX 专用逻辑使用）
    virtual void* GetPxPhysics() const = 0;
    virtual void* GetPxScene() const = 0;
    virtual void* GetPxCooking() const = 0;
};

} // namespace physics3d
} // namespace dse
