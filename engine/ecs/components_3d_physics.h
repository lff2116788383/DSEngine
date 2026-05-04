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

/// 角色控制器碰撞标志位（对应 PhysX PxControllerCollisionFlag）
enum class CharacterCollisionFlag : uint8_t {
    None    = 0,
    Sides   = 1 << 0,   ///< 与侧面碰撞
    Up      = 1 << 1,   ///< 与上方碰撞
    Down    = 1 << 2,   ///< 与下方碰撞（着地）
};

inline CharacterCollisionFlag operator|(CharacterCollisionFlag a, CharacterCollisionFlag b) {
    return static_cast<CharacterCollisionFlag>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline bool operator&(CharacterCollisionFlag a, CharacterCollisionFlag b) {
    return (static_cast<uint8_t>(a) & static_cast<uint8_t>(b)) != 0;
}

/// 角色控制器组件 — 基于 kinematic PxRigidDynamic + sweep 的运动学角色控制
struct CharacterController3DComponent {
    float radius = 0.3f;            ///< 胶囊半径
    float height = 1.0f;            ///< 胶囊高度（不含半球）
    float slope_limit = 45.0f;      ///< 不可行走坡度限制（度）
    float step_offset = 0.3f;       ///< 台阶高度偏移
    float skin_width = 0.01f;       ///< 碰撞皮肤宽度
    float min_move_distance = 0.0f; ///< 最小移动距离阈值

    // 运行时状态
    bool is_grounded = false;       ///< 是否着地
    glm::vec3 velocity = glm::vec3(0.0f);      ///< 当前速度
    CharacterCollisionFlag collision_flags = CharacterCollisionFlag::None;  ///< 碰撞标志

    // Backend handle — PxRigidDynamic*（kinematic 角色代理）
    void* runtime_controller = nullptr;
};

} // namespace dse

#endif // DSE_COMPONENTS_3D_PHYSICS_H