#ifndef DSE_PHYSICS3D_SYSTEM_H
#define DSE_PHYSICS3D_SYSTEM_H

#include "engine/ecs/world.h"
#include <glm/glm.hpp>
#include <memory>

namespace physx {
    class PxFoundation;
    class PxPhysics;
    class PxScene;
    class PxCpuDispatcher;
    class PxMaterial;
}

namespace dse {
namespace physics3d {

struct RaycastResult {
    bool hit = false;
    entt::entity entity = entt::null;
    glm::vec3 hit_point = glm::vec3(0.0f);
    glm::vec3 hit_normal = glm::vec3(0.0f);
    float distance = 0.0f;
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

private:
    physx::PxFoundation* foundation_ = nullptr;
    physx::PxPhysics* physics_ = nullptr;
    physx::PxScene* scene_ = nullptr;
    physx::PxCpuDispatcher* dispatcher_ = nullptr;
    physx::PxMaterial* default_material_ = nullptr;
    World* world_cache_ = nullptr; ///< Init 时缓存的 World 指针，供动力学 API 使用

    void SyncTransformsToPhysics(World& world);
    void SyncPhysicsToTransforms(World& world);
};

} // namespace physics3d
} // namespace dse

#endif // DSE_PHYSICS3D_SYSTEM_H