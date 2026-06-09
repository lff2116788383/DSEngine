/**
 * @file dse_api_physics3d.cpp
 * @brief DSEngine Native C ABI — 3D 物理服务（L5，手写）
 *
 * L5：依赖 Physics3D 服务（ServiceLocator / IPhysics3DSystem）+ ECS 碰撞体回退，
 * 无法由 codegen 表达（非纯组件字段访问）。在此手写 C ABI，使 Lua / C# / 编辑器
 * 三端共享同一 raycast 实现。Lua L_Physics3DRaycast 退化为薄包装委托本函数。
 *
 * raycast 语义（与原 Lua 实现逐行等价）：
 *   1) direction 内部归一化；len<=eps 或 max_dist<=0 → 未命中。
 *   2) DSE_HAS_PHYSICS3D 且服务已注册 → 先走加速 Raycast，命中即返回。
 *   3) 否则（或加速未命中）回退遍历 Box/Sphere ECS 碰撞体取最近命中。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/core/service_locator.h"
#include "engine/physics/physics3d/i_physics3d_system.h"

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

using Entity = entt::entity;
using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }

struct RaycastHit {
    bool      hit      = false;
    Entity    entity   = entt::null;
    glm::vec3 point    = glm::vec3(0.0f);
    glm::vec3 normal   = glm::vec3(0.0f);
    float     distance = 0.0f;
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

RaycastHit RaycastEcs3DColliders(World& world, const glm::vec3& origin,
                                 const glm::vec3& direction, float max_dist) {
    RaycastHit best;
    best.distance = max_dist;

    auto box_view = world.registry().view<TransformComponent, dse::BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& transform = box_view.get<TransformComponent>(entity);
        const auto& collider = box_view.get<dse::BoxCollider3DComponent>(entity);
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

    auto sphere_view = world.registry().view<TransformComponent, dse::SphereCollider3DComponent>();
    for (auto entity : sphere_view) {
        const auto& transform = sphere_view.get<TransformComponent>(entity);
        const auto& collider = sphere_view.get<dse::SphereCollider3DComponent>(entity);
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

} // namespace

// ============================================================
// dse_physics3d_raycast — L5 服务 raycast（PhysX/Jolt 加速 + ECS 回退）
// ============================================================
extern "C" int dse_physics3d_raycast(float ox, float oy, float oz,
                                     float dx, float dy, float dz,
                                     float max_dist,
                                     uint32_t* out_entity,
                                     float* out_point,
                                     float* out_normal,
                                     float* out_distance) {
    World* world = GW();
    if (!world) return 0;

    glm::vec3 origin(ox, oy, oz);
    glm::vec3 direction(dx, dy, dz);

    const float len = glm::length(direction);
    if (len <= 1.0e-6f || max_dist <= 0.0f) return 0;
    direction /= len;

    RaycastHit best;

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto physx_hit = physics->Raycast(origin, direction, max_dist);
        if (physx_hit.hit) {
            best.hit = true;
            best.entity = physx_hit.entity;
            best.point = physx_hit.hit_point;
            best.normal = physx_hit.hit_normal;
            best.distance = physx_hit.distance;
        }
    }
#endif

    if (!best.hit) {
        best = RaycastEcs3DColliders(*world, origin, direction, max_dist);
    }

    if (!best.hit) return 0;

    if (out_entity)   *out_entity = static_cast<uint32_t>(static_cast<entt::id_type>(best.entity));
    if (out_point)  { out_point[0] = best.point.x;  out_point[1] = best.point.y;  out_point[2] = best.point.z; }
    if (out_normal) { out_normal[0] = best.normal.x; out_normal[1] = best.normal.y; out_normal[2] = best.normal.z; }
    if (out_distance) *out_distance = best.distance;
    return 1;
}

// ============================================================
// dse_rigidbody3d_* — L5 RigidBody 动力学（服务委托 + 组件缓存）
// ============================================================

extern "C" void dse_rigidbody3d_add_force(uint32_t e, float fx, float fy, float fz) {
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->AddForce(TE(e), glm::vec3(fx, fy, fz));
    }
#endif
}

extern "C" void dse_rigidbody3d_add_impulse(uint32_t e, float ix, float iy, float iz) {
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->AddImpulse(TE(e), glm::vec3(ix, iy, iz));
    }
#endif
}

extern "C" void dse_rigidbody3d_add_torque(uint32_t e, float tx, float ty, float tz) {
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->AddTorque(TE(e), glm::vec3(tx, ty, tz));
    }
#endif
}

extern "C" void dse_rigidbody3d_set_velocity(uint32_t e, float vx, float vy, float vz) {
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetVelocity(TE(e), glm::vec3(vx, vy, vz));
    }
#endif
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) rb->velocity = glm::vec3(vx, vy, vz);
}

extern "C" void dse_rigidbody3d_get_velocity(uint32_t e, float* out_vel) {
    if (!out_vel) return;
    out_vel[0] = out_vel[1] = out_vel[2] = 0.0f;
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        glm::vec3 v = physics->GetVelocity(TE(e));
        out_vel[0] = v.x; out_vel[1] = v.y; out_vel[2] = v.z;
        return;
    }
#endif
    const auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) { out_vel[0] = rb->velocity.x; out_vel[1] = rb->velocity.y; out_vel[2] = rb->velocity.z; }
}

extern "C" void dse_rigidbody3d_set_angular_velocity(uint32_t e, float ax, float ay, float az) {
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetAngularVelocity(TE(e), glm::vec3(ax, ay, az));
    }
#endif
}

extern "C" void dse_rigidbody3d_get_angular_velocity(uint32_t e, float* out_vel) {
    if (!out_vel) return;
    out_vel[0] = out_vel[1] = out_vel[2] = 0.0f;
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        glm::vec3 v = physics->GetAngularVelocity(TE(e));
        out_vel[0] = v.x; out_vel[1] = v.y; out_vel[2] = v.z;
    }
#endif
}

extern "C" void dse_rigidbody3d_set_gravity(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetGravityEnabled(TE(e), enabled != 0);
    }
#endif
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) rb->use_gravity = (enabled != 0);
}

// ============================================================
// dse_character_controller3d_* — L5 角色控制器（服务 + ECS 回退）
// ============================================================

extern "C" int dse_character_controller3d_move(uint32_t e, float dx, float dy, float dz,
                                               float min_dist, float dt,
                                               float* out_velocity, uint32_t* out_flags) {
    World* world = GW();
    if (!world) {
        if (out_velocity) { out_velocity[0] = out_velocity[1] = out_velocity[2] = 0.0f; }
        if (out_flags) *out_flags = 0;
        return 0;
    }

    Entity entity = TE(e);

#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        auto result = physics->MoveCharacter(entity, glm::vec3(dx, dy, dz), min_dist, dt);
        // 地形贴地补正（Jolt CharacterVirtual 不感知 ECS 高度图）
        auto* tf = world->registry().try_get<TransformComponent>(entity);
        if (tf) {
            float terrain_y = -1e10f;
            auto hm_view = world->registry().view<TerrainHeightmapComponent>();
            for (auto te : hm_view) {
                const auto& hm = hm_view.get<TerrainHeightmapComponent>(te);
                float h = hm.GetHeight(tf->position.x, tf->position.z);
                if (h > terrain_y) terrain_y = h;
            }
            if (terrain_y > -1e9f && tf->position.y < terrain_y) {
                tf->position.y = terrain_y;
                tf->dirty = true;
                result.is_grounded = true;
                result.collision_flags = static_cast<uint8_t>(result.collision_flags)
                    | static_cast<uint8_t>(CharacterCollisionFlag::Down);
            }
        }
        if (out_velocity) {
            out_velocity[0] = result.velocity.x;
            out_velocity[1] = result.velocity.y;
            out_velocity[2] = result.velocity.z;
        }
        if (out_flags) *out_flags = static_cast<uint32_t>(result.collision_flags);
        return result.is_grounded ? 1 : 0;
    }
#endif

    // ECS 回退（无物理服务）
    auto* transform = world->registry().try_get<TransformComponent>(entity);
    if (!transform) {
        if (out_velocity) { out_velocity[0] = out_velocity[1] = out_velocity[2] = 0.0f; }
        if (out_flags) *out_flags = 0;
        return 0;
    }

    auto* cc = world->registry().try_get<CharacterController3DComponent>(entity);
    glm::vec3 original_pos = transform->position;
    glm::vec3 new_pos = original_pos + glm::vec3(dx, dy, dz);
    uint8_t cflags = 0;

    float char_radius = cc ? cc->radius : 0.3f;
    float char_half_h = cc ? cc->height * 0.5f : 0.5f;
    glm::vec3 sphere_c = new_pos + glm::vec3(0, char_radius + char_half_h, 0);

    // Box 碰撞体推开
    auto box_view = world->registry().view<TransformComponent, dse::BoxCollider3DComponent>();
    for (auto other : box_view) {
        if (other == entity) continue;
        const auto& bt = box_view.get<TransformComponent>(other);
        const auto& bc = box_view.get<dse::BoxCollider3DComponent>(other);
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

    // Sphere 碰撞体推开
    auto sph_view = world->registry().view<TransformComponent, dse::SphereCollider3DComponent>();
    for (auto other : sph_view) {
        if (other == entity) continue;
        const auto& st = sph_view.get<TransformComponent>(other);
        const auto& sc = sph_view.get<dse::SphereCollider3DComponent>(other);
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

    if (out_velocity) { out_velocity[0] = vel.x; out_velocity[1] = vel.y; out_velocity[2] = vel.z; }
    if (out_flags) *out_flags = static_cast<uint32_t>(cflags);
    return grounded ? 1 : 0;
}

extern "C" int dse_character_controller3d_jump(uint32_t e, float jump_speed) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        return physics->JumpCharacter(TE(e), jump_speed) ? 1 : 0;
    }
#endif
    return 0;
}

extern "C" int dse_character_controller3d_is_grounded(uint32_t e) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        return physics->IsCharacterGrounded(TE(e)) ? 1 : 0;
    }
#endif
    World* world = GW();
    if (world) {
        const auto* cc = world->registry().try_get<CharacterController3DComponent>(TE(e));
        if (cc) return cc->is_grounded ? 1 : 0;
    }
    return 0;
}

extern "C" int dse_character_controller3d_get_position(uint32_t e, float* out_xyz) {
    if (!out_xyz) return 0;
    out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        glm::vec3 pos = physics->GetCharacterPosition(TE(e));
        out_xyz[0] = pos.x; out_xyz[1] = pos.y; out_xyz[2] = pos.z;
        return 1;
    }
#endif
    World* world = GW();
    if (world) {
        const auto* tf = world->registry().try_get<TransformComponent>(TE(e));
        if (tf) {
            out_xyz[0] = tf->position.x; out_xyz[1] = tf->position.y; out_xyz[2] = tf->position.z;
            return 1;
        }
    }
    return 0;
}

// ============================================================
// dse_*_add — L3 组件创建（ECS emplace_or_replace，逐值等价于原 Lua 内联）
// ============================================================

extern "C" void dse_rigidbody3d_add(uint32_t e, int type, float mass) {
    World* world = GW();
    if (!world) return;
    auto& rb = world->registry().emplace_or_replace<RigidBody3DComponent>(TE(e));
    rb.type = static_cast<RigidBody3DType>(type);
    rb.mass = mass;
}

extern "C" void dse_box_collider3d_add(uint32_t e, float x, float y, float z) {
    World* world = GW();
    if (!world) return;
    auto& c = world->registry().emplace_or_replace<BoxCollider3DComponent>(TE(e));
    c.size = glm::vec3(x, y, z);
}

extern "C" void dse_sphere_collider3d_add(uint32_t e, float radius) {
    World* world = GW();
    if (!world) return;
    auto& c = world->registry().emplace_or_replace<SphereCollider3DComponent>(TE(e));
    c.radius = radius;
}

extern "C" void dse_capsule_collider3d_add(uint32_t e, float radius, float height,
                                           int direction, int is_trigger) {
    World* world = GW();
    if (!world) return;
    auto& c = world->registry().emplace_or_replace<CapsuleCollider3DComponent>(TE(e));
    c.radius = radius;
    c.height = height;
    c.direction = direction;
    c.is_trigger = (is_trigger != 0);
}

extern "C" void dse_mesh_collider3d_add(uint32_t e, int convex, int is_trigger) {
    World* world = GW();
    if (!world) return;
    auto& c = world->registry().emplace_or_replace<MeshCollider3DComponent>(TE(e));
    c.convex = (convex != 0);
    c.is_trigger = (is_trigger != 0);
}

extern "C" void dse_character_controller3d_add(uint32_t e, float radius, float height,
                                               float slope_limit, float step_offset) {
    World* world = GW();
    if (!world) return;
    auto& cc = world->registry().emplace_or_replace<CharacterController3DComponent>(TE(e));
    cc.radius = radius;
    cc.height = height;
    cc.slope_limit = slope_limit;
    cc.step_offset = step_offset;
}

extern "C" void dse_joint3d_add(uint32_t e, uint32_t connected_id, int type,
                                float ax, float ay, float az,
                                float bx, float by, float bz,
                                float break_force, float break_torque) {
    World* world = GW();
    if (!world) return;
    auto& jc = world->registry().emplace_or_replace<Joint3DComponent>(TE(e));
    jc.type = static_cast<Joint3DType>(type);
    jc.connected_entity_id = connected_id;
    jc.anchor = glm::vec3(ax, ay, az);
    jc.connected_anchor = glm::vec3(bx, by, bz);
    jc.break_force = break_force;
    jc.break_torque = break_torque;
}

extern "C" void dse_terrain_heightmap_add(uint32_t e, float origin_x, float origin_z,
                                          float block_size, int cols, int rows,
                                          float scale, int flip_z) {
    World* world = GW();
    if (!world) return;
    auto& hm = world->registry().emplace_or_replace<TerrainHeightmapComponent>(TE(e));
    hm.origin_x = origin_x;
    hm.origin_z = origin_z;
    hm.block_size = block_size;
    hm.cols = cols;
    hm.rows = rows;
    hm.scale = scale;
    hm.flip_z = (flip_z != 0);
}

// ============================================================
// dse_terrain_* — 高度图数据写入 / 查询
// ============================================================

extern "C" void dse_terrain_heightmap_set_data(uint32_t e, const float* heights, int count) {
    World* world = GW();
    if (!world) return;
    auto* hm = world->registry().try_get<TerrainHeightmapComponent>(TE(e));
    if (!hm) return;
    hm->heights.assign(count > 0 ? count : 0, 0.0f);
    for (int i = 0; i < count && heights; ++i) hm->heights[i] = heights[i];
}

extern "C" int dse_terrain_get_height(float world_x, float world_z, float* out_y) {
    if (out_y) *out_y = 0.0f;
    World* world = GW();
    if (!world) return 0;
    float best_y = 0.0f;
    bool found = false;
    auto view = world->registry().view<TerrainHeightmapComponent>();
    for (auto te : view) {
        const auto& hm = view.get<TerrainHeightmapComponent>(te);
        float h = hm.GetHeight(world_x, world_z);
        if (!found || h > best_y) { best_y = h; found = true; }
    }
    if (out_y) *out_y = best_y;
    return found ? 1 : 0;
}

// ============================================================
// dse_joint3d_* — 关节附加参数 setter / 查询
// ============================================================

extern "C" void dse_joint3d_set_hinge_limits(uint32_t e, float lower_deg, float upper_deg) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint3DComponent>(TE(e));
    if (!jc) return;
    jc->use_limits = true;
    jc->lower_limit = lower_deg;
    jc->upper_limit = upper_deg;
}

extern "C" void dse_joint3d_set_spring(uint32_t e, float stiffness, float damping) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint3DComponent>(TE(e));
    if (!jc) return;
    jc->spring_stiffness = stiffness;
    jc->spring_damping = damping;
}

extern "C" void dse_joint3d_set_distance(uint32_t e, float min_dist, float max_dist) {
    World* world = GW();
    if (!world) return;
    auto* jc = world->registry().try_get<Joint3DComponent>(TE(e));
    if (!jc) return;
    jc->min_distance = min_dist;
    jc->max_distance = max_dist;
}

extern "C" int dse_joint3d_is_broken(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* jc = world->registry().try_get<Joint3DComponent>(TE(e));
    return (jc && jc->is_broken) ? 1 : 0;
}

// ============================================================
// dse_collision_* / dse_collider_* — 碰撞层 / trigger / 材质
// ============================================================

extern "C" void dse_collision_set_layer(uint32_t e, int layer, int mask) {
    World* world = GW();
    if (!world) return;
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (!rb) return;
    rb->collision_layer = static_cast<uint16_t>(layer);
    rb->collision_mask = static_cast<uint16_t>(mask);
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetCollisionLayer(TE(e), static_cast<uint16_t>(layer), static_cast<uint16_t>(mask));
    }
#endif
}

extern "C" void dse_collider_set_trigger(uint32_t e, int is_trigger) {
    World* world = GW();
    if (!world) return;
    const bool trigger = (is_trigger != 0);
    if (auto* box = world->registry().try_get<BoxCollider3DComponent>(TE(e))) box->is_trigger = trigger;
    if (auto* sph = world->registry().try_get<SphereCollider3DComponent>(TE(e))) sph->is_trigger = trigger;
    if (auto* cap = world->registry().try_get<CapsuleCollider3DComponent>(TE(e))) cap->is_trigger = trigger;
    if (auto* mc = world->registry().try_get<MeshCollider3DComponent>(TE(e))) mc->is_trigger = trigger;
}

extern "C" void dse_collider_set_material(uint32_t e, float friction, float bounciness) {
    World* world = GW();
    if (!world) return;
    if (auto* box = world->registry().try_get<BoxCollider3DComponent>(TE(e))) {
        box->friction = friction; box->bounciness = bounciness;
    }
    if (auto* sph = world->registry().try_get<SphereCollider3DComponent>(TE(e))) {
        sph->friction = friction; sph->bounciness = bounciness;
    }
    if (auto* cap = world->registry().try_get<CapsuleCollider3DComponent>(TE(e))) {
        cap->friction = friction; cap->bounciness = bounciness;
    }
    if (auto* mc = world->registry().try_get<MeshCollider3DComponent>(TE(e))) {
        mc->friction = friction; mc->bounciness = bounciness;
    }
}

// ============================================================
// dse_physics3d_overlap_* — 重叠查询（ECS 几何，写入 out 数组，返回命中数）
// ============================================================

extern "C" int dse_physics3d_overlap_sphere(float cx, float cy, float cz, float radius,
                                            uint32_t* out, int cap) {
    World* world = GW();
    if (!world) return 0;
    const glm::vec3 center(cx, cy, cz);
    const float r2 = radius * radius;
    int count = 0;
    auto emit = [&](Entity entity) {
        if (out && count < cap) out[count] = static_cast<uint32_t>(static_cast<entt::id_type>(entity));
        ++count;
    };

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
        if (glm::dot(diff, diff) <= r2) emit(entity);
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
        if (glm::dot(diff, diff) <= combined * combined) emit(entity);
    }
    return count;
}

extern "C" int dse_physics3d_overlap_box(float min_x, float min_y, float min_z,
                                         float max_x, float max_y, float max_z,
                                         uint32_t* out, int cap) {
    World* world = GW();
    if (!world) return 0;
    const glm::vec3 qmin(min_x, min_y, min_z);
    const glm::vec3 qmax(max_x, max_y, max_z);
    int count = 0;
    auto emit = [&](Entity entity) {
        if (out && count < cap) out[count] = static_cast<uint32_t>(static_cast<entt::id_type>(entity));
        ++count;
    };

    auto box_view = world->registry().view<TransformComponent, BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& t = box_view.get<TransformComponent>(entity);
        const auto& c = box_view.get<BoxCollider3DComponent>(entity);
        glm::vec3 bc = t.position + c.center;
        glm::vec3 half = glm::abs(t.scale * c.size) * 0.5f;
        glm::vec3 bmin = bc - half, bmax = bc + half;
        if (qmin.x <= bmax.x && qmax.x >= bmin.x &&
            qmin.y <= bmax.y && qmax.y >= bmin.y &&
            qmin.z <= bmax.z && qmax.z >= bmin.z) emit(entity);
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
        if (glm::dot(diff, diff) <= sr * sr) emit(entity);
    }
    return count;
}
