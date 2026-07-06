/**
 * @file dse_api_physics3d_ext.cpp
 * @brief DSEngine Native C ABI — Physics3D 扩展（Collision/Trigger events, SphereCast, BoxCast, Sleep/Wake, Kinematic, Mass, Damping）
 *
 * 补齐 Lua 有而 C ABI 缺失的 Physics3D 运行时 API。
 * 碰撞/触发事件通过 IPhysics3DSystem 获取；SphereCast/BoxCast 在无 PhysX 时回退 ECS。
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

} // namespace

// ============================================================
// Collision / Trigger events
// ============================================================

extern "C" int dse_physics3d_get_collision_count(void) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        return static_cast<int>(physics->GetCollisionEvents().size());
    }
#endif
    return 0;
}

extern "C" int dse_physics3d_get_collision_events(float* out_buf, int cap) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto& events = physics->GetCollisionEvents();
        int count = static_cast<int>(events.size());
        if (count > cap) count = cap;
        for (int i = 0; i < count; ++i) {
            const auto& e = events[i];
            float* p = out_buf + i * 11;
            p[0] = static_cast<float>(static_cast<int>(e.type));
            p[1] = static_cast<float>(static_cast<uint32_t>(e.entity_a));
            p[2] = static_cast<float>(static_cast<uint32_t>(e.entity_b));
            p[3] = e.contact_point.x; p[4] = e.contact_point.y; p[5] = e.contact_point.z;
            p[6] = e.contact_normal.x; p[7] = e.contact_normal.y; p[8] = e.contact_normal.z;
            p[9] = e.impulse;
            p[10] = 0.0f; // padding
        }
        return count;
    }
#endif
    return 0;
}

extern "C" int dse_physics3d_get_trigger_count(void) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        return static_cast<int>(physics->GetTriggerEvents().size());
    }
#endif
    return 0;
}

extern "C" int dse_physics3d_get_trigger_events(uint32_t* out_entities, int* out_types, int cap) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        const auto& events = physics->GetTriggerEvents();
        int count = static_cast<int>(events.size());
        if (count > cap) count = cap;
        for (int i = 0; i < count; ++i) {
            const auto& e = events[i];
            if (out_entities) {
                out_entities[i * 2]     = static_cast<uint32_t>(e.trigger_entity);
                out_entities[i * 2 + 1] = static_cast<uint32_t>(e.other_entity);
            }
            if (out_types) out_types[i] = static_cast<int>(e.type);
        }
        return count;
    }
#endif
    return 0;
}

// ============================================================
// SphereCast / BoxCast
// ============================================================

extern "C" int dse_physics3d_spherecast(float ox, float oy, float oz,
                                        float dx, float dy, float dz,
                                        float radius, float max_dist,
                                        uint32_t* out_entity, float* out_point,
                                        float* out_normal, float* out_distance) {
    World* world = GW();
    if (!world) return 0;
    glm::vec3 origin(ox, oy, oz);
    glm::vec3 direction(dx, dy, dz);
    const float len = glm::length(direction);
    if (len <= 1.0e-6f || max_dist <= 0.0f) return 0;
    direction /= len;

    // ECS fallback: treat as raycast with sphere radius expansion on colliders
    float best_t = max_dist;
    Entity best_entity = entt::null;
    glm::vec3 best_point(0.0f), best_normal(0.0f);
    bool hit = false;

    auto box_view = world->registry().view<TransformComponent, dse::BoxCollider3DComponent>();
    for (auto entity : box_view) {
        const auto& tf = box_view.get<TransformComponent>(entity);
        const auto& col = box_view.get<dse::BoxCollider3DComponent>(entity);
        glm::vec3 center = tf.position + col.center;
        glm::vec3 half = glm::abs(tf.scale * col.size) * 0.5f + glm::vec3(radius);
        // Ray-AABB test with expanded box
        float tmin = 0.0f, tmax = best_t;
        for (int i = 0; i < 3; ++i) {
            float o = origin[i], d = direction[i];
            float mn = center[i] - half[i], mx = center[i] + half[i];
            if (std::fabs(d) < 1e-6f) { if (o < mn || o > mx) continue; }
            else {
                float t1 = (mn - o) / d, t2 = (mx - o) / d;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
            }
        }
        if (tmin <= tmax && tmin < best_t && tmin >= 0.0f) {
            best_t = tmin; best_entity = entity; hit = true;
            best_point = origin + direction * tmin;
            best_normal = glm::normalize(best_point - center);
        }
    }

    auto sph_view = world->registry().view<TransformComponent, dse::SphereCollider3DComponent>();
    for (auto entity : sph_view) {
        const auto& tf = sph_view.get<TransformComponent>(entity);
        const auto& col = sph_view.get<dse::SphereCollider3DComponent>(entity);
        glm::vec3 center = tf.position + col.center;
        float max_s = std::max(std::fabs(tf.scale.x), std::max(std::fabs(tf.scale.y), std::fabs(tf.scale.z)));
        float r = col.radius * max_s + radius;
        glm::vec3 oc = origin - center;
        float b = 2.0f * glm::dot(oc, direction);
        float c = glm::dot(oc, oc) - r * r;
        float disc = b * b - 4.0f * c;
        if (disc < 0.0f) continue;
        float t = (-b - std::sqrt(disc)) * 0.5f;
        if (t < 0.0f) t = (-b + std::sqrt(disc)) * 0.5f;
        if (t >= 0.0f && t < best_t) {
            best_t = t; best_entity = entity; hit = true;
            best_point = origin + direction * t;
            best_normal = glm::normalize(best_point - center);
        }
    }

    if (!hit) return 0;
    if (out_entity) *out_entity = static_cast<uint32_t>(static_cast<entt::id_type>(best_entity));
    if (out_point) { out_point[0] = best_point.x; out_point[1] = best_point.y; out_point[2] = best_point.z; }
    if (out_normal) { out_normal[0] = best_normal.x; out_normal[1] = best_normal.y; out_normal[2] = best_normal.z; }
    if (out_distance) *out_distance = best_t;
    return 1;
}

extern "C" int dse_physics3d_boxcast(float ox, float oy, float oz,
                                     float dx, float dy, float dz,
                                     float hx, float hy, float hz,
                                     float max_dist,
                                     uint32_t* out_entity, float* out_point,
                                     float* out_normal, float* out_distance) {
    // BoxCast is approximated as a SphereCast with the largest half-extent as radius
    // (full swept AABB would require more complex Minkowski expansion)
    float radius = std::max(hx, std::max(hy, hz));
    return dse_physics3d_spherecast(ox, oy, oz, dx, dy, dz, radius, max_dist,
                                    out_entity, out_point, out_normal, out_distance);
}

// ============================================================
// RigidBody3D extras
// ============================================================

extern "C" void dse_rigidbody3d_sleep(uint32_t e) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SleepBody(TE(e));
    }
#endif
}

extern "C" void dse_rigidbody3d_wake(uint32_t e) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->WakeBody(TE(e));
    }
#endif
}

extern "C" void dse_rigidbody3d_set_kinematic(uint32_t e, int kinematic) {
    World* world = GW();
    if (!world) return;
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) {
        rb->type = kinematic ? RigidBody3DType::Kinematic : RigidBody3DType::Dynamic;
    }
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetKinematic(TE(e), kinematic != 0);
    }
#endif
}

extern "C" float dse_rigidbody3d_get_mass(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    return rb ? rb->mass : 0.0f;
}

extern "C" void dse_rigidbody3d_set_mass(uint32_t e, float mass) {
    World* world = GW();
    if (!world) return;
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) rb->mass = mass;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetMass(TE(e), mass);
    }
#endif
}

extern "C" void dse_rigidbody3d_add_force_at_position(uint32_t e, float fx, float fy, float fz,
                                                       float px, float py, float pz) {
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->AddForceAtPosition(TE(e), glm::vec3(fx, fy, fz), glm::vec3(px, py, pz));
    }
#endif
}

extern "C" void dse_rigidbody3d_set_linear_damping(uint32_t e, float damping) {
    World* world = GW();
    if (!world) return;
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) rb->linear_damping = damping;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetLinearDamping(TE(e), damping);
    }
#endif
}

extern "C" float dse_rigidbody3d_get_linear_damping(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    return rb ? rb->linear_damping : 0.0f;
}

extern "C" void dse_rigidbody3d_set_angular_damping(uint32_t e, float damping) {
    World* world = GW();
    if (!world) return;
    auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    if (rb) rb->angular_damping = damping;
#ifdef DSE_HAS_PHYSICS3D
    if (auto* physics = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>()) {
        physics->SetAngularDamping(TE(e), damping);
    }
#endif
}

extern "C" float dse_rigidbody3d_get_angular_damping(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* rb = world->registry().try_get<RigidBody3DComponent>(TE(e));
    return rb ? rb->angular_damping : 0.0f;
}
