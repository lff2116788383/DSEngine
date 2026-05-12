#ifndef DSE_PHYSICS3D_SYSTEM_H
#define DSE_PHYSICS3D_SYSTEM_H

#include "engine/ecs/world.h"
#include <glm/glm.hpp>
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

/// 碰撞事件（Task 1）
struct CollisionEvent {
    enum class Type { Enter, Stay, Exit };
    Type type = Type::Enter;
    entt::entity entity_a = entt::null;
    entt::entity entity_b = entt::null;
    glm::vec3 contact_point = glm::vec3(0.0f);
    glm::vec3 contact_normal = glm::vec3(0.0f);
    float impulse = 0.0f;
};

/// 触发事件（Task 1）
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

/// 角色控制器移动结果
struct CharacterMoveResult {
    bool is_grounded = false;           ///< 是否着地
    glm::vec3 velocity = glm::vec3(0.0f);  ///< 移动后速度
    uint8_t collision_flags = 0;        ///< CharacterCollisionFlag 位掩码
};

class Physics3DSystem {
public:
    Physics3DSystem() = default;
    ~Physics3DSystem() = default;

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

    // 角色控制器 API（基于 PxScene::sweep 的自定义实现，不依赖 PxControllerManager）
    CharacterMoveResult MoveCharacter(entt::entity entity, const glm::vec3& displacement, float min_dist, float delta_time);
    bool JumpCharacter(entt::entity entity, float jump_speed);
    bool IsCharacterGrounded(entt::entity entity) const;
    glm::vec3 GetCharacterPosition(entt::entity entity) const;

    // 碰撞/触发事件查询 API（Task 1）
    const std::vector<CollisionEvent>& GetCollisionEvents() const { return collision_events_; }
    const std::vector<TriggerEvent>& GetTriggerEvents() const { return trigger_events_; }

    // 碰撞层设置 API（Task 6）
    void SetCollisionLayer(entt::entity entity, uint16_t layer, uint16_t mask);

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