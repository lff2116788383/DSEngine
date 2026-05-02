/**
 * @file lua_binding_ecs_phys3d.cpp
 * @brief ECS Lua 绑定 — 3D 物理（RigidBody3D、碰撞体、Raycast）
 *
 * @note 文件名使用 phys3d 而非 physics3d，以规避 CMake 中
 *       DSE_ENABLE_PHYSX=OFF 时对 physics3d 模式的 GLOB 过滤。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/core/service_locator.h"
#ifdef DSE_ENABLE_PHYSX
#include "engine/physics/physics3d/physics3d_system.h"
#endif
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <algorithm>
#include <cmath>

namespace dse::runtime::lua_binding {
namespace {

int L_EcsAddRigidBody3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int type = helper::OptInt(L, 2, 2);
    float mass = helper::OptFloat(L, 3, 1.0f);
    auto& rb = world->registry().emplace_or_replace<RigidBody3DComponent>(e);
    rb.type = static_cast<RigidBody3DType>(type);
    rb.mass = mass;
    return 0;
}

int L_EcsAddBoxCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float x = helper::CheckFloat(L, 2);
    float y = helper::CheckFloat(L, 3);
    float z = helper::CheckFloat(L, 4);
    auto& collider = world->registry().emplace_or_replace<BoxCollider3DComponent>(e);
    collider.size = glm::vec3(x, y, z);
    return 0;
}

int L_EcsAddSphereCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::CheckFloat(L, 2);
    auto& collider = world->registry().emplace_or_replace<SphereCollider3DComponent>(e);
    collider.radius = radius;
    return 0;
}

/// 对 3D 刚体施加持续的力（需 PhysX 后端）
int L_EcsRigidBody3DAddForce(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float fx = helper::CheckFloat(L, 2);
    float fy = helper::CheckFloat(L, 3);
    float fz = helper::CheckFloat(L, 4);
#ifdef DSE_ENABLE_PHYSX
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>()) {
        physics->AddForce(e, glm::vec3(fx, fy, fz));
    }
#endif
    return 0;
}

/// 对 3D 刚体施加瞬时冲量（需 PhysX 后端）
int L_EcsRigidBody3DAddImpulse(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float ix = helper::CheckFloat(L, 2);
    float iy = helper::CheckFloat(L, 3);
    float iz = helper::CheckFloat(L, 4);
#ifdef DSE_ENABLE_PHYSX
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>()) {
        physics->AddImpulse(e, glm::vec3(ix, iy, iz));
    }
#endif
    return 0;
}

/// 设置 3D 刚体线速度（需 PhysX 后端）
int L_EcsRigidBody3DSetVelocity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float vx = helper::CheckFloat(L, 2);
    float vy = helper::CheckFloat(L, 3);
    float vz = helper::CheckFloat(L, 4);
#ifdef DSE_ENABLE_PHYSX
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>()) {
        physics->SetVelocity(e, glm::vec3(vx, vy, vz));
    }
#endif
    // 同步到组件缓存
    auto* rb = helper::TryGetComponent<RigidBody3DComponent>(*world, e);
    if (rb) {
        rb->velocity = glm::vec3(vx, vy, vz);
    }
    return 0;
}

/// 获取 3D 刚体线速度（需 PhysX 后端），返回 vx,vy,vz
int L_EcsRigidBody3DGetVelocity(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
        return 3;
    }
    Entity e = helper::CheckEntity(L, 1);
#ifdef DSE_ENABLE_PHYSX
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>()) {
        glm::vec3 vel = physics->GetVelocity(e);
        helper::PushVec3(L, vel);
        return 3;
    }
#endif
    // 无 PhysX 时回退到组件缓存
    const auto* rb = helper::TryGetComponentConst<RigidBody3DComponent>(*world, e);
    if (rb) {
        helper::PushVec3(L, rb->velocity);
    } else {
        lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    }
    return 3;
}

/// 设置 3D 刚体是否受重力（需 PhysX 后端）
int L_EcsRigidBody3DSetGravity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool enabled = helper::CheckBool(L, 2);
#ifdef DSE_ENABLE_PHYSX
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>()) {
        physics->SetGravityEnabled(e, enabled);
    }
#endif
    auto* rb = helper::TryGetComponent<RigidBody3DComponent>(*world, e);
    if (rb) {
        rb->use_gravity = enabled;
    }
    return 0;
}

// ============================================================
// 3D Raycast — 本地碰撞体回退 + PhysX 加速
// ============================================================

struct LuaPhysicsRaycastHit {
    bool hit = false;
    Entity entity = entt::null;
    glm::vec3 point = glm::vec3(0.0f);
    glm::vec3 normal = glm::vec3(0.0f);
    float distance = 0.0f;
};

bool IntersectRayAabb(const glm::vec3& origin, const glm::vec3& dir,
                       const glm::vec3& min_v, const glm::vec3& max_v,
                       float max_dist, float& out_t, glm::vec3& out_normal) {
    float tmin = 0.0f;
    float tmax = max_dist;
    glm::vec3 enter_normal(0.0f);

    auto test_axis = [&](float o, float d, float min_axis, float max_axis,
                         const glm::vec3& negative_normal, const glm::vec3& positive_normal) -> bool {
        const float eps = 1.0e-6f;
        if (std::fabs(d) < eps) {
            return o >= min_axis && o <= max_axis;
        }
        float t1 = (min_axis - o) / d;
        float t2 = (max_axis - o) / d;
        glm::vec3 n1 = negative_normal;
        glm::vec3 n2 = positive_normal;
        if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
        if (t1 > tmin) { tmin = t1; enter_normal = n1; }
        if (t2 < tmax) { tmax = t2; }
        return tmin <= tmax;
    };

    if (!test_axis(origin.x, dir.x, min_v.x, max_v.x, glm::vec3(-1,0,0), glm::vec3(1,0,0))) return false;
    if (!test_axis(origin.y, dir.y, min_v.y, max_v.y, glm::vec3(0,-1,0), glm::vec3(0,1,0))) return false;
    if (!test_axis(origin.z, dir.z, min_v.z, max_v.z, glm::vec3(0,0,-1), glm::vec3(0,0,1))) return false;

    out_t = tmin;
    out_normal = enter_normal;
    return out_t >= 0.0f && out_t <= max_dist;
}

bool IntersectRaySphere(const glm::vec3& origin, const glm::vec3& dir,
                         const glm::vec3& center, float radius,
                         float max_dist, float& out_t, glm::vec3& out_normal) {
    glm::vec3 oc = origin - center;
    const float b = 2.0f * glm::dot(oc, dir);
    const float c = glm::dot(oc, oc) - radius * radius;
    const float discriminant = b * b - 4.0f * c;
    if (discriminant < 0.0f) return false;
    const float root = std::sqrt(discriminant);
    float t = (-b - root) * 0.5f;
    if (t < 0.0f) t = (-b + root) * 0.5f;
    if (t < 0.0f || t > max_dist) return false;
    out_t = t;
    out_normal = glm::normalize((origin + dir * t) - center);
    return true;
}

LuaPhysicsRaycastHit RaycastEcs3DColliders(World& world, const glm::vec3& origin,
                                            const glm::vec3& direction, float max_dist) {
    LuaPhysicsRaycastHit best;
    best.distance = max_dist;

    auto box_view = world.registry().view<TransformComponent, BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& transform = box_view.get<TransformComponent>(entity);
        const auto& collider = box_view.get<BoxCollider3DComponent>(entity);
        const glm::vec3 center = transform.position + collider.center;
        const glm::vec3 half_size = glm::abs(transform.scale * collider.size) * 0.5f;
        float t = 0.0f;
        glm::vec3 normal(0.0f);
        if (IntersectRayAabb(origin, direction, center - half_size, center + half_size, best.distance, t, normal)) {
            best.hit = true;
            best.entity = entity;
            best.distance = t;
            best.point = origin + direction * t;
            best.normal = normal;
        }
    }

    auto sphere_view = world.registry().view<TransformComponent, SphereCollider3DComponent>();
    for (auto entity : sphere_view) {
        const auto& transform = sphere_view.get<TransformComponent>(entity);
        const auto& collider = sphere_view.get<SphereCollider3DComponent>(entity);
        const glm::vec3 center = transform.position + collider.center;
        const float max_scale = std::max(std::fabs(transform.scale.x),
                                          std::max(std::fabs(transform.scale.y), std::fabs(transform.scale.z)));
        float t = 0.0f;
        glm::vec3 normal(0.0f);
        if (IntersectRaySphere(origin, direction, center, collider.radius * max_scale, best.distance, t, normal)) {
            best.hit = true;
            best.entity = entity;
            best.distance = t;
            best.point = origin + direction * t;
            best.normal = normal;
        }
    }

    return best;
}

int PushPhysics3DRaycastResult(lua_State* L, const LuaPhysicsRaycastHit& hit) {
    lua_pushboolean(L, hit.hit ? 1 : 0);
    if (!hit.hit) return 1;
    helper::PushEntity(L, hit.entity);
    helper::PushVec3(L, hit.point);
    helper::PushVec3(L, hit.normal);
    helper::PushFloat(L, hit.distance);
    return 9;
}

int L_Physics3DRaycast(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushboolean(L, 0);
        return 1;
    }

    glm::vec3 origin = helper::CheckVec3(L, 1);
    glm::vec3 direction = helper::CheckVec3(L, 4);
    float max_dist = helper::OptFloat(L, 7, 1000.0f);

    const float len = glm::length(direction);
    if (len <= 1.0e-6f || max_dist <= 0.0f) {
        lua_pushboolean(L, 0);
        return 1;
    }
    direction /= len;

#ifdef DSE_ENABLE_PHYSX
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>()) {
        const auto physx_hit = physics->Raycast(origin, direction, max_dist);
        if (physx_hit.hit) {
            LuaPhysicsRaycastHit hit;
            hit.hit = true;
            hit.entity = physx_hit.entity;
            hit.point = physx_hit.hit_point;
            hit.normal = physx_hit.hit_normal;
            hit.distance = physx_hit.distance;
            return PushPhysics3DRaycastResult(L, hit);
        }
    }
#endif

    return PushPhysics3DRaycastResult(L, RaycastEcs3DColliders(*world, origin, direction, max_dist));
}

} // namespace

void RegisterEcsPhysics3DBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_rigidbody_3d",          L_EcsAddRigidBody3D},
        {"add_box_collider_3d",       L_EcsAddBoxCollider3D},
        {"add_sphere_collider_3d",    L_EcsAddSphereCollider3D},
        {"rigidbody_3d_add_force",    L_EcsRigidBody3DAddForce},
        {"rigidbody_3d_add_impulse",  L_EcsRigidBody3DAddImpulse},
        {"rigidbody_3d_set_velocity", L_EcsRigidBody3DSetVelocity},
        {"rigidbody_3d_get_velocity", L_EcsRigidBody3DGetVelocity},
        {"rigidbody_3d_set_gravity",  L_EcsRigidBody3DSetGravity},
        {"physics_3d_raycast",        L_Physics3DRaycast},
    });
}

} // namespace dse::runtime::lua_binding
