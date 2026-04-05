#ifndef DSE_COMPONENTS_3D_PHYSICS_H
#define DSE_COMPONENTS_3D_PHYSICS_H

#include <glm/glm.hpp>
#include <vector>

namespace dse {

enum class RigidBody3DType {
    Static = 0,
    Kinematic = 1,
    Dynamic = 2
};

struct RigidBody3DComponent {
    RigidBody3DType type = RigidBody3DType::Dynamic;
    glm::vec3 velocity = glm::vec3(0.0f);
    glm::vec3 angular_velocity = glm::vec3(0.0f);
    float mass = 1.0f;
    float drag = 0.0f;
    float angular_drag = 0.05f;
    bool use_gravity = true;
    float gravity_scale = 1.0f;
    bool is_kinematic = false;

    // Backend handle
    void* runtime_body = nullptr;
};

struct BoxCollider3DComponent {
    glm::vec3 size = glm::vec3(1.0f);
    glm::vec3 center = glm::vec3(0.0f);
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Backend handle
    void* runtime_shape = nullptr;
};

struct SphereCollider3DComponent {
    float radius = 0.5f;
    glm::vec3 center = glm::vec3(0.0f);
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Backend handle
    void* runtime_shape = nullptr;
};

struct MeshCollider3DComponent {
    bool convex = false;
    bool is_trigger = false;
    float bounciness = 0.0f;
    float friction = 0.5f;

    // Backend handle
    void* runtime_shape = nullptr;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_PHYSICS_H