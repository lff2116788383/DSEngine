#ifndef DSE_PHYSICS3D_SYSTEM_H
#define DSE_PHYSICS3D_SYSTEM_H

#include "engine/ecs/world.h"
#include <glm/glm.hpp>
#include <memory>

namespace physx {
    class PxFoundation;
    class PxPhysics;
    class PxScene;
    class PxDefaultCpuDispatcher;
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
    
    // API for Raycasting
    RaycastResult Raycast(const glm::vec3& origin, const glm::vec3& direction, float max_distance);

private:
    physx::PxFoundation* foundation_ = nullptr;
    physx::PxPhysics* physics_ = nullptr;
    physx::PxScene* scene_ = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher_ = nullptr;
    physx::PxMaterial* default_material_ = nullptr;

    void SyncTransformsToPhysics(World& world);
    void SyncPhysicsToTransforms(World& world);
};

} // namespace physics3d
} // namespace dse

#endif // DSE_PHYSICS3D_SYSTEM_H