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
