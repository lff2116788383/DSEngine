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
#include "engine/physics/physics3d/i_physics3d_system.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

#include <algorithm>
#include <cmath>
#include <cfloat>

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
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
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
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->AddImpulse(e, glm::vec3(ix, iy, iz));
    }
#endif
    return 0;
}

/// 对 3D 刚体施加扭矩
int L_EcsRigidBody3DAddTorque(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float tx = helper::CheckFloat(L, 2);
    float ty = helper::CheckFloat(L, 3);
    float tz = helper::CheckFloat(L, 4);
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->AddTorque(e, glm::vec3(tx, ty, tz));
    }
#endif
    return 0;
}

/// 设置 3D 刚体角速度
int L_EcsRigidBody3DSetAngularVelocity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float ax = helper::CheckFloat(L, 2);
    float ay = helper::CheckFloat(L, 3);
    float az = helper::CheckFloat(L, 4);
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetAngularVelocity(e, glm::vec3(ax, ay, az));
    }
#endif
    return 0;
}

/// 获取 3D 刚体角速度，返回 ax,ay,az
int L_EcsRigidBody3DGetAngularVelocity(lua_State* L) {
    World* world = GetWorld();
    if (!world) {
        lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
        return 3;
    }
    Entity e = helper::CheckEntity(L, 1);
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        glm::vec3 vel = physics->GetAngularVelocity(e);
        helper::PushVec3(L, vel);
        return 3;
    }
#endif
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 3;
}

/// 设置 3D 刚体线速度（需 PhysX 后端）
int L_EcsRigidBody3DSetVelocity(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float vx = helper::CheckFloat(L, 2);
    float vy = helper::CheckFloat(L, 3);
    float vz = helper::CheckFloat(L, 4);
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
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
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
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
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
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

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
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

// ============================================================
// CharacterController3D 绑定
// ============================================================

/// 添加角色控制器组件：add_character_controller_3d(entity, radius, height, [slope_limit, step_offset])
int L_EcsAddCharacterController3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::OptFloat(L, 2, 0.3f);
    float height = helper::OptFloat(L, 3, 1.0f);
    float slope_limit = helper::OptFloat(L, 4, 45.0f);
    float step_offset = helper::OptFloat(L, 5, 0.3f);
    auto& cc = world->registry().emplace_or_replace<CharacterController3DComponent>(e);
    cc.radius = radius;
    cc.height = height;
    cc.slope_limit = slope_limit;
    cc.step_offset = step_offset;
    lua_pushboolean(L, 1);
    return 1;
}

/// 移动角色控制器：character_controller_3d_move(entity, dx, dy, dz, [min_dist, dt]) → is_grounded, vel_x, vel_y, vel_z, collision_flags
int L_EcsCharacterController3DMove(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float dx = helper::CheckFloat(L, 2);
    float dy = helper::CheckFloat(L, 3);
    float dz = helper::CheckFloat(L, 4);
    float min_dist = helper::OptFloat(L, 5, 0.0f);
    float dt = helper::OptFloat(L, 6, 1.0f / 60.0f);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        auto result = physics->MoveCharacter(e, glm::vec3(dx, dy, dz), min_dist, dt);
        // Jolt CharacterVirtual 不知道 ECS 地形高度图，补上地形贴地检查
        World* world_phys = GetWorld();
        if (world_phys) {
            auto* tf_phys = helper::TryGetComponent<TransformComponent>(*world_phys, e);
            if (tf_phys) {
                float terrain_y = -1e10f;
                auto hm_view = world_phys->registry().view<TerrainHeightmapComponent>();
                for (auto te : hm_view) {
                    const auto& hm = hm_view.get<TerrainHeightmapComponent>(te);
                    float h = hm.GetHeight(tf_phys->position.x, tf_phys->position.z);
                    if (h > terrain_y) terrain_y = h;
                }
                if (terrain_y > -1e9f && tf_phys->position.y < terrain_y) {
                    tf_phys->position.y = terrain_y;
                    tf_phys->dirty = true;
                    result.is_grounded = true;
                    result.collision_flags = static_cast<uint8_t>(result.collision_flags)
                        | static_cast<uint8_t>(CharacterCollisionFlag::Down);
                }
            }
        }
        lua_pushboolean(L, result.is_grounded ? 1 : 0);
        helper::PushVec3(L, result.velocity);
        lua_pushinteger(L, static_cast<lua_Integer>(result.collision_flags));
        return 5;
    }
#endif
    // 无 PhysX 时回退：地形贴地 + 碰撞体推开 + 着地检测
    World* world = GetWorld();
    if (world) {
        auto* transform = helper::TryGetComponent<TransformComponent>(*world, e);
        if (transform) {
            auto* cc = helper::TryGetComponent<CharacterController3DComponent>(*world, e);
            glm::vec3 original_pos = transform->position;
            glm::vec3 new_pos = original_pos + glm::vec3(dx, dy, dz);
            uint8_t cflags = 0;

            // 碰撞体推开 — 将角色视为球体
            float char_radius = cc ? cc->radius : 0.3f;
            float char_half_h = cc ? cc->height * 0.5f : 0.5f;
            glm::vec3 sphere_c = new_pos + glm::vec3(0, char_radius + char_half_h, 0);

            // Box 碰撞体
            auto box_view = world->registry().view<TransformComponent, BoxCollider3DComponent>();
            for (auto other : box_view) {
                if (other == e) continue;
                const auto& bt = box_view.get<TransformComponent>(other);
                const auto& bc = box_view.get<BoxCollider3DComponent>(other);
                glm::vec3 bc_center = bt.position + bc.center;
                glm::vec3 half = glm::abs(bt.scale * bc.size) * 0.5f;
                glm::vec3 bmin = bc_center - half;
                glm::vec3 bmax = bc_center + half;
                glm::vec3 closest;
                closest.x = std::max(bmin.x, std::min(sphere_c.x, bmax.x));
                closest.y = std::max(bmin.y, std::min(sphere_c.y, bmax.y));
                closest.z = std::max(bmin.z, std::min(sphere_c.z, bmax.z));
                glm::vec3 diff = sphere_c - closest;
                float dist_sq = glm::dot(diff, diff);
                if (dist_sq < char_radius * char_radius && dist_sq > 1e-8f) {
                    float d = std::sqrt(dist_sq);
                    glm::vec3 n = diff / d;
                    float overlap = char_radius - d;
                    new_pos += n * overlap;
                    sphere_c += n * overlap;
                    cflags |= static_cast<uint8_t>(CharacterCollisionFlag::Sides);
                }
            }

            // Sphere 碰撞体
            auto sph_view = world->registry().view<TransformComponent, SphereCollider3DComponent>();
            for (auto other : sph_view) {
                if (other == e) continue;
                const auto& st = sph_view.get<TransformComponent>(other);
                const auto& sc = sph_view.get<SphereCollider3DComponent>(other);
                glm::vec3 oc = st.position + sc.center;
                float max_s = std::max(std::fabs(st.scale.x),
                              std::max(std::fabs(st.scale.y), std::fabs(st.scale.z)));
                float or_ = sc.radius * max_s;
                glm::vec3 diff = sphere_c - oc;
                float d = glm::length(diff);
                float md = char_radius + or_;
                if (d < md && d > 1e-6f) {
                    glm::vec3 n = diff / d;
                    float overlap_val = md - d;
                    new_pos += n * overlap_val;
                    sphere_c += n * overlap_val;
                    cflags |= static_cast<uint8_t>(CharacterCollisionFlag::Sides);
                }
            }

            // 地形高度贴地
            bool grounded = false;
            float terrain_y = -1e10f;
            auto hm_view = world->registry().view<TerrainHeightmapComponent>();
            for (auto te : hm_view) {
                const auto& hm = hm_view.get<TerrainHeightmapComponent>(te);
                float h = hm.GetHeight(new_pos.x, new_pos.z);
                if (h > terrain_y) terrain_y = h;
            }
            if (terrain_y > -1e9f && new_pos.y <= terrain_y) {
                new_pos.y = terrain_y;
                grounded = true;
                cflags |= static_cast<uint8_t>(CharacterCollisionFlag::Down);
            }

            transform->position = new_pos;
            transform->dirty = true;

            glm::vec3 vel = (dt > 1e-6f) ? (new_pos - original_pos) / dt : glm::vec3(0);
            if (cc) {
                cc->is_grounded = grounded;
                cc->collision_flags = static_cast<CharacterCollisionFlag>(cflags);
                cc->velocity = vel;
            }

            lua_pushboolean(L, grounded ? 1 : 0);
            helper::PushVec3(L, vel);
            lua_pushinteger(L, static_cast<lua_Integer>(cflags));
            return 5;
        }
    }
    lua_pushboolean(L, 0);
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    lua_pushinteger(L, 0);
    return 5;
}

/// 角色跳跃：character_controller_3d_jump(entity, jump_speed) → success
int L_EcsCharacterController3DJump(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);
    float jump_speed = helper::OptFloat(L, 2, 5.0f);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        bool success = physics->JumpCharacter(e, jump_speed);
        lua_pushboolean(L, success ? 1 : 0);
        return 1;
    }
#endif
    lua_pushboolean(L, 0);
    return 1;
}

/// 查询是否着地：character_controller_3d_is_grounded(entity) → is_grounded
int L_EcsCharacterController3DIsGrounded(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        lua_pushboolean(L, physics->IsCharacterGrounded(e) ? 1 : 0);
        return 1;
    }
#endif
    // 无 PhysX 回退：从组件缓存读取
    World* world = GetWorld();
    if (world) {
        const auto* cc = helper::TryGetComponentConst<CharacterController3DComponent>(*world, e);
        if (cc) {
            lua_pushboolean(L, cc->is_grounded ? 1 : 0);
            return 1;
        }
    }
    lua_pushboolean(L, 0);
    return 1;
}

/// 获取角色位置：character_controller_3d_get_position(entity) → x, y, z
int L_EcsCharacterController3DGetPosition(lua_State* L) {
    Entity e = helper::CheckEntity(L, 1);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        glm::vec3 pos = physics->GetCharacterPosition(e);
        helper::PushVec3(L, pos);
        return 3;
    }
#endif
    // 回退到 Transform
    World* world = GetWorld();
    if (world) {
        const auto* transform = helper::TryGetComponentConst<TransformComponent>(*world, e);
        if (transform) {
            helper::PushVec3(L, transform->position);
            return 3;
        }
    }
    lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0); lua_pushnumber(L, 0.0);
    return 3;
}

// ============================================================
// TerrainHeightmap 绑定
// ============================================================

/// add_terrain_heightmap(entity, origin_x, origin_z, block_size, cols, rows, [scale, flip_z])
int L_EcsAddTerrainHeightmap(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float ox = helper::CheckFloat(L, 2);
    float oz = helper::CheckFloat(L, 3);
    float bs = helper::CheckFloat(L, 4);
    int cols = helper::CheckInt(L, 5);
    int rows = helper::CheckInt(L, 6);
    float sc = helper::OptFloat(L, 7, 1.0f);
    bool fz = helper::OptBool(L, 8, false);
    auto& hm = world->registry().emplace_or_replace<TerrainHeightmapComponent>(e);
    hm.origin_x = ox;
    hm.origin_z = oz;
    hm.block_size = bs;
    hm.cols = cols;
    hm.rows = rows;
    hm.scale = sc;
    hm.flip_z = fz;
    return 0;
}

/// terrain_heightmap_set_data(entity, table)
int L_EcsTerrainHeightmapSetData(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* hm = helper::TryGetComponent<TerrainHeightmapComponent>(*world, e);
    if (!hm) return 0;
    luaL_checktype(L, 2, LUA_TTABLE);
    int n = static_cast<int>(lua_rawlen(L, 2));
    hm->heights.resize(n);
    for (int i = 0; i < n; ++i) {
        lua_rawgeti(L, 2, i + 1);
        hm->heights[i] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    return 0;
}

/// terrain_get_height(world_x, world_z) -> height_y
int L_EcsTerrainGetHeight(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushnumber(L, 0.0); return 1; }
    float wx = helper::CheckFloat(L, 1);
    float wz = helper::CheckFloat(L, 2);
    float best_y = 0.0f;
    bool found = false;
    auto view = world->registry().view<TerrainHeightmapComponent>();
    for (auto te : view) {
        const auto& hm = view.get<TerrainHeightmapComponent>(te);
        float h = hm.GetHeight(wx, wz);
        if (!found || h > best_y) { best_y = h; found = true; }
    }
    helper::PushFloat(L, best_y);
    return 1;
}

// ============================================================
// 碰撞/触发事件查询（Task 1）
// ============================================================

/// physics_3d_get_collision_events() -> table of {type, entity_a, entity_b, px, py, pz, nx, ny, nz, impulse}
int L_Physics3DGetCollisionEvents(lua_State* L) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto& events = physics->GetCollisionEvents();
        lua_newtable(L);
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& e = events[i];
            lua_newtable(L);
            lua_pushinteger(L, static_cast<int>(e.type));
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.entity_a)));
            lua_setfield(L, -2, "entity_a");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.entity_b)));
            lua_setfield(L, -2, "entity_b");
            lua_pushnumber(L, e.contact_point.x); lua_setfield(L, -2, "px");
            lua_pushnumber(L, e.contact_point.y); lua_setfield(L, -2, "py");
            lua_pushnumber(L, e.contact_point.z); lua_setfield(L, -2, "pz");
            lua_pushnumber(L, e.contact_normal.x); lua_setfield(L, -2, "nx");
            lua_pushnumber(L, e.contact_normal.y); lua_setfield(L, -2, "ny");
            lua_pushnumber(L, e.contact_normal.z); lua_setfield(L, -2, "nz");
            lua_pushnumber(L, e.impulse); lua_setfield(L, -2, "impulse");
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }
#endif
    lua_newtable(L);
    return 1;
}

/// physics_3d_get_trigger_events() -> table of {type, trigger_entity, other_entity}
int L_Physics3DGetTriggerEvents(lua_State* L) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto& events = physics->GetTriggerEvents();
        lua_newtable(L);
        for (size_t i = 0; i < events.size(); ++i) {
            const auto& e = events[i];
            lua_newtable(L);
            lua_pushinteger(L, static_cast<int>(e.type));
            lua_setfield(L, -2, "type");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.trigger_entity)));
            lua_setfield(L, -2, "trigger_entity");
            lua_pushinteger(L, static_cast<lua_Integer>(static_cast<uint32_t>(e.other_entity)));
            lua_setfield(L, -2, "other_entity");
            lua_rawseti(L, -2, static_cast<int>(i + 1));
        }
        return 1;
    }
#endif
    lua_newtable(L);
    return 1;
}

// ============================================================
// MeshCollider3D 绑定（Task 3）
// ============================================================

/// add_mesh_collider_3d(entity, convex, [is_trigger])
int L_EcsAddMeshCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool convex = helper::OptBool(L, 2, false);
    bool is_trigger = helper::OptBool(L, 3, false);
    auto& mc = world->registry().emplace_or_replace<MeshCollider3DComponent>(e);
    mc.convex = convex;
    mc.is_trigger = is_trigger;
    return 0;
}

// ============================================================
// CapsuleCollider3D 绑定（Task 4）
// ============================================================

/// add_capsule_collider_3d(entity, radius, height, [direction, is_trigger])
int L_EcsAddCapsuleCollider3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float radius = helper::CheckFloat(L, 2);
    float height = helper::CheckFloat(L, 3);
    int direction = helper::OptInt(L, 4, 1); // default Y-axis
    bool is_trigger = helper::OptBool(L, 5, false);
    auto& cap = world->registry().emplace_or_replace<CapsuleCollider3DComponent>(e);
    cap.radius = radius;
    cap.height = height;
    cap.direction = direction;
    cap.is_trigger = is_trigger;
    return 0;
}

// ============================================================
// Joint3D 绑定（Task 5）
// ============================================================

/// add_joint_3d(entity_a, entity_b_id, type, ax,ay,az, bx,by,bz, [break_force, break_torque])
int L_EcsAddJoint3D(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int connected_id = helper::CheckInt(L, 2);
    int type = helper::OptInt(L, 3, 0);
    float ax = helper::OptFloat(L, 4, 0.0f);
    float ay = helper::OptFloat(L, 5, 0.0f);
    float az = helper::OptFloat(L, 6, 0.0f);
    float bx = helper::OptFloat(L, 7, 0.0f);
    float by = helper::OptFloat(L, 8, 0.0f);
    float bz = helper::OptFloat(L, 9, 0.0f);
    float bf = helper::OptFloat(L, 10, FLT_MAX);
    float bt = helper::OptFloat(L, 11, FLT_MAX);

    auto& jc = world->registry().emplace_or_replace<Joint3DComponent>(e);
    jc.type = static_cast<Joint3DType>(type);
    jc.connected_entity_id = static_cast<uint32_t>(connected_id);
    jc.anchor = glm::vec3(ax, ay, az);
    jc.connected_anchor = glm::vec3(bx, by, bz);
    jc.break_force = bf;
    jc.break_torque = bt;
    return 0;
}

/// set_joint_3d_hinge_limits(entity, lower_deg, upper_deg)
int L_EcsSetJoint3DHingeLimits(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint3DComponent>(*world, e);
    if (!jc) return 0;
    jc->use_limits = true;
    jc->lower_limit = helper::CheckFloat(L, 2);
    jc->upper_limit = helper::CheckFloat(L, 3);
    return 0;
}

/// set_joint_3d_spring(entity, stiffness, damping)
int L_EcsSetJoint3DSpring(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint3DComponent>(*world, e);
    if (!jc) return 0;
    jc->spring_stiffness = helper::CheckFloat(L, 2);
    jc->spring_damping = helper::CheckFloat(L, 3);
    return 0;
}

/// set_joint_3d_distance(entity, min_dist, max_dist)
int L_EcsSetJoint3DDistance(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    auto* jc = helper::TryGetComponent<Joint3DComponent>(*world, e);
    if (!jc) return 0;
    jc->min_distance = helper::CheckFloat(L, 2);
    jc->max_distance = helper::CheckFloat(L, 3);
    return 0;
}

/// is_joint_3d_broken(entity) -> bool
int L_EcsIsJoint3DBroken(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, 0); return 1; }
    Entity e = helper::CheckEntity(L, 1);
    const auto* jc = helper::TryGetComponentConst<Joint3DComponent>(*world, e);
    lua_pushboolean(L, (jc && jc->is_broken) ? 1 : 0);
    return 1;
}

// ============================================================
// 碰撞层设置（Task 6）
// ============================================================

/// set_collision_layer(entity, layer, mask)
int L_EcsSetCollisionLayer(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    int layer = helper::CheckInt(L, 2);
    int mask = helper::CheckInt(L, 3);

    auto* rb = helper::TryGetComponent<RigidBody3DComponent>(*world, e);
    if (!rb) return 0;
    rb->collision_layer = static_cast<uint16_t>(layer);
    rb->collision_mask = static_cast<uint16_t>(mask);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetCollisionLayer(e, static_cast<uint16_t>(layer), static_cast<uint16_t>(mask));
    }
#endif
    return 0;
}

/// set_collider_trigger(entity, is_trigger) — 设置碰撞体 trigger 标志
int L_EcsSetColliderTrigger(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    bool trigger = helper::CheckBool(L, 2);
    if (auto* box = helper::TryGetComponent<BoxCollider3DComponent>(*world, e)) box->is_trigger = trigger;
    if (auto* sph = helper::TryGetComponent<SphereCollider3DComponent>(*world, e)) sph->is_trigger = trigger;
    if (auto* cap = helper::TryGetComponent<CapsuleCollider3DComponent>(*world, e)) cap->is_trigger = trigger;
    if (auto* mc = helper::TryGetComponent<MeshCollider3DComponent>(*world, e)) mc->is_trigger = trigger;
    return 0;
}

/// set_collider_material(entity, friction, bounciness)
int L_EcsSetColliderMaterial(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = helper::CheckEntity(L, 1);
    float friction = helper::CheckFloat(L, 2);
    float bounciness = helper::CheckFloat(L, 3);
    if (auto* box = helper::TryGetComponent<BoxCollider3DComponent>(*world, e)) {
        box->friction = friction; box->bounciness = bounciness;
    }
    if (auto* sph = helper::TryGetComponent<SphereCollider3DComponent>(*world, e)) {
        sph->friction = friction; sph->bounciness = bounciness;
    }
    if (auto* cap = helper::TryGetComponent<CapsuleCollider3DComponent>(*world, e)) {
        cap->friction = friction; cap->bounciness = bounciness;
    }
    if (auto* mc = helper::TryGetComponent<MeshCollider3DComponent>(*world, e)) {
        mc->friction = friction; mc->bounciness = bounciness;
    }
    return 0;
}

// ============================================================
// Overlap Query 绑定
// ============================================================

/// physics_3d_overlap_sphere(cx, cy, cz, radius) -> {entity1, entity2, ...}
int L_Physics3DOverlapSphere(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_newtable(L); return 1; }
    float cx = helper::CheckFloat(L, 1);
    float cy = helper::CheckFloat(L, 2);
    float cz = helper::CheckFloat(L, 3);
    float radius = helper::CheckFloat(L, 4);
    glm::vec3 center(cx, cy, cz);
    float r2 = radius * radius;

    lua_newtable(L);
    int idx = 1;

    auto box_view = world->registry().view<TransformComponent, BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& t = box_view.get<TransformComponent>(entity);
        const auto& c = box_view.get<BoxCollider3DComponent>(entity);
        glm::vec3 bc = t.position + c.center;
        glm::vec3 half = glm::abs(t.scale * c.size) * 0.5f;
        glm::vec3 bmin = bc - half, bmax = bc + half;
        glm::vec3 closest;
        closest.x = std::max(bmin.x, std::min(center.x, bmax.x));
        closest.y = std::max(bmin.y, std::min(center.y, bmax.y));
        closest.z = std::max(bmin.z, std::min(center.z, bmax.z));
        glm::vec3 diff = center - closest;
        if (glm::dot(diff, diff) <= r2) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    auto sph_view = world->registry().view<TransformComponent, SphereCollider3DComponent>();
    for (auto entity : sph_view) {
        const auto& t = sph_view.get<TransformComponent>(entity);
        const auto& c = sph_view.get<SphereCollider3DComponent>(entity);
        glm::vec3 sc = t.position + c.center;
        float max_s = std::max(std::fabs(t.scale.x),
                      std::max(std::fabs(t.scale.y), std::fabs(t.scale.z)));
        float sr = c.radius * max_s;
        float combined = radius + sr;
        glm::vec3 diff = center - sc;
        if (glm::dot(diff, diff) <= combined * combined) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    return 1;
}

/// physics_3d_overlap_box(min_x, min_y, min_z, max_x, max_y, max_z) -> {entity1, entity2, ...}
int L_Physics3DOverlapBox(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_newtable(L); return 1; }
    glm::vec3 qmin = helper::CheckVec3(L, 1);
    glm::vec3 qmax = helper::CheckVec3(L, 4);

    lua_newtable(L);
    int idx = 1;

    auto box_view = world->registry().view<TransformComponent, BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& t = box_view.get<TransformComponent>(entity);
        const auto& c = box_view.get<BoxCollider3DComponent>(entity);
        glm::vec3 bc = t.position + c.center;
        glm::vec3 half = glm::abs(t.scale * c.size) * 0.5f;
        glm::vec3 bmin = bc - half, bmax = bc + half;
        if (qmin.x <= bmax.x && qmax.x >= bmin.x &&
            qmin.y <= bmax.y && qmax.y >= bmin.y &&
            qmin.z <= bmax.z && qmax.z >= bmin.z) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    auto sph_view = world->registry().view<TransformComponent, SphereCollider3DComponent>();
    for (auto entity : sph_view) {
        const auto& t = sph_view.get<TransformComponent>(entity);
        const auto& c = sph_view.get<SphereCollider3DComponent>(entity);
        glm::vec3 sc = t.position + c.center;
        float max_s = std::max(std::fabs(t.scale.x),
                      std::max(std::fabs(t.scale.y), std::fabs(t.scale.z)));
        float sr = c.radius * max_s;
        glm::vec3 closest;
        closest.x = std::max(qmin.x, std::min(sc.x, qmax.x));
        closest.y = std::max(qmin.y, std::min(sc.y, qmax.y));
        closest.z = std::max(qmin.z, std::min(sc.z, qmax.z));
        glm::vec3 diff = sc - closest;
        if (glm::dot(diff, diff) <= sr * sr) {
            helper::PushEntity(L, entity);
            lua_rawseti(L, -2, idx++);
        }
    }

    return 1;
}

} // namespace

void RegisterEcsPhysics3DBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"add_rigidbody_3d",          L_EcsAddRigidBody3D},
        {"add_box_collider_3d",       L_EcsAddBoxCollider3D},
        {"add_sphere_collider_3d",    L_EcsAddSphereCollider3D},
        {"rigidbody_3d_add_force",            L_EcsRigidBody3DAddForce},
        {"rigidbody_3d_add_torque",           L_EcsRigidBody3DAddTorque},
        {"rigidbody_3d_add_impulse",          L_EcsRigidBody3DAddImpulse},
        {"rigidbody_3d_set_velocity",         L_EcsRigidBody3DSetVelocity},
        {"rigidbody_3d_get_velocity",         L_EcsRigidBody3DGetVelocity},
        {"rigidbody_3d_set_angular_velocity", L_EcsRigidBody3DSetAngularVelocity},
        {"rigidbody_3d_get_angular_velocity", L_EcsRigidBody3DGetAngularVelocity},
        {"rigidbody_3d_set_gravity",          L_EcsRigidBody3DSetGravity},
        {"physics_3d_raycast",        L_Physics3DRaycast},
        {"add_character_controller_3d",          L_EcsAddCharacterController3D},
        {"character_controller_3d_move",         L_EcsCharacterController3DMove},
        {"character_controller_3d_jump",         L_EcsCharacterController3DJump},
        {"character_controller_3d_is_grounded",  L_EcsCharacterController3DIsGrounded},
        {"character_controller_3d_get_position", L_EcsCharacterController3DGetPosition},
        {"add_terrain_heightmap",          L_EcsAddTerrainHeightmap},
        {"terrain_heightmap_set_data",     L_EcsTerrainHeightmapSetData},
        {"terrain_get_height",             L_EcsTerrainGetHeight},
        {"physics_3d_overlap_sphere",      L_Physics3DOverlapSphere},
        {"physics_3d_overlap_box",         L_Physics3DOverlapBox},
        // Task 1: Collision/Trigger events
        {"physics_3d_get_collision_events", L_Physics3DGetCollisionEvents},
        {"physics_3d_get_trigger_events",   L_Physics3DGetTriggerEvents},
        // Task 3: MeshCollider3D
        {"add_mesh_collider_3d",            L_EcsAddMeshCollider3D},
        // Task 4: CapsuleCollider3D
        {"add_capsule_collider_3d",         L_EcsAddCapsuleCollider3D},
        // Task 5: Joints
        {"add_joint_3d",                    L_EcsAddJoint3D},
        {"set_joint_3d_hinge_limits",       L_EcsSetJoint3DHingeLimits},
        {"set_joint_3d_spring",             L_EcsSetJoint3DSpring},
        {"set_joint_3d_distance",           L_EcsSetJoint3DDistance},
        {"is_joint_3d_broken",              L_EcsIsJoint3DBroken},
        // Task 6: Collision layers
        {"set_collision_layer",             L_EcsSetCollisionLayer},
        // Helpers
        {"set_collider_trigger",            L_EcsSetColliderTrigger},
        {"set_collider_material",           L_EcsSetColliderMaterial},
    });
}

} // namespace dse::runtime::lua_binding
