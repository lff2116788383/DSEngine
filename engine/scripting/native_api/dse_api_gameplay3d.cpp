/**
 * @file dse_api_gameplay3d.cpp
 * @brief DSEngine Native C ABI — Gameplay3D 物理系统（破碎 / 布料 / 流体，手写）
 *
 * S1.9 L5 收敛：将 Fracture / Cloth / Fluid 的「模拟控制 + 复合 setter」逻辑从
 * Lua C++ 上移到手写 C ABI，操作全局 World（dse_get_world_ptr），使 Lua / C# /
 * 编辑器三端共享同一实现。对应 Lua 绑定退化为薄包装委托本文件函数，零行为变更。
 *
 * 可选参数语义：原 Lua 中以 OptFloat(L, n, 当前值) 实现的「省略即保持当前值」字段，
 * 在 C ABI 中用 NaN 哨兵表示「保持当前值」（Lua 薄包装对省略参数传入 NaN）。
 * 固定默认值（如 add_* 的默认、a=0.8）由 Lua 侧在调用前解析，C ABI 直接写入。
 */

#include "engine/scripting/native_api/dse_api.h"

#include "engine/ecs/world.h"
#include "engine/ecs/components_3d_fracture.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fluid.h"

#include <glm/glm.hpp>
#include <cmath>

using Entity = entt::entity;
using namespace dse;

namespace {

inline World* GW() { return static_cast<World*>(dse_get_world_ptr()); }
inline Entity TE(uint32_t e) { return static_cast<Entity>(static_cast<entt::id_type>(e)); }
inline bool Keep(float v) { return std::isnan(v); }  // NaN => 保持当前值

}  // namespace

// ============================================================
// Fracture — 可破坏实体
// ============================================================

extern "C" void dse_fracture_add(uint32_t e, int source, uint32_t fragment_count,
                                 float break_force, float health) {
    World* world = GW();
    if (!world) return;
    auto& fc = world->registry().emplace_or_replace<FractureComponent>(TE(e));
    fc.source = static_cast<FractureSource>(source);
    fc.runtime_fragment_count = fragment_count;
    fc.break_force = break_force;
    fc.health = health;
    fc.max_health = health;
}

extern "C" void dse_fracture_set_params(uint32_t e, float explosion_force, float fragment_lifetime,
                                        float fade_duration, float mass_scale) {
    World* world = GW();
    if (!world) return;
    auto* fc = world->registry().try_get<FractureComponent>(TE(e));
    if (!fc) return;
    if (!Keep(explosion_force))   fc->explosion_force = explosion_force;
    if (!Keep(fragment_lifetime)) fc->fragment_lifetime = fragment_lifetime;
    if (!Keep(fade_duration))     fc->fragment_fade_duration = fade_duration;
    if (!Keep(mass_scale))        fc->fragment_mass_scale = mass_scale;
}

extern "C" void dse_fracture_apply_damage(uint32_t e, float damage, float ix, float iy, float iz) {
    World* world = GW();
    if (!world) return;
    auto* fc = world->registry().try_get<FractureComponent>(TE(e));
    if (!fc) return;
    fc->health -= damage;
    if (fc->health <= 0.0f && !fc->is_fractured) {
        fc->fracture_requested = true;
        fc->impact_point = glm::vec3(ix, iy, iz);
    }
}

extern "C" void dse_fracture_trigger(uint32_t e, float ix, float iy, float iz) {
    World* world = GW();
    if (!world) return;
    auto* fc = world->registry().try_get<FractureComponent>(TE(e));
    if (!fc || fc->is_fractured) return;
    fc->fracture_requested = true;
    fc->impact_point = glm::vec3(ix, iy, iz);
}

extern "C" int dse_fracture_is_fractured(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* fc = world->registry().try_get<FractureComponent>(TE(e));
    return (fc && fc->is_fractured) ? 1 : 0;
}

// ============================================================
// Cloth — 布料模拟
// ============================================================

extern "C" void dse_cloth_add(uint32_t e, uint32_t solver_iterations, float stiffness,
                              float damping, float bend_stiffness) {
    World* world = GW();
    if (!world) return;
    auto& cloth = world->registry().emplace_or_replace<ClothComponent>(TE(e));
    cloth.enabled = true;
    cloth.solver_iterations = solver_iterations;
    cloth.stiffness = stiffness;
    cloth.damping = damping;
    cloth.bend_stiffness = bend_stiffness;
}

extern "C" void dse_cloth_set_wind(uint32_t e, float wx, float wy, float wz, float turbulence) {
    World* world = GW();
    if (!world) return;
    auto* cloth = world->registry().try_get<ClothComponent>(TE(e));
    if (!cloth) return;
    cloth->wind = glm::vec3(wx, wy, wz);
    if (!Keep(turbulence)) cloth->wind_turbulence = turbulence;
}

extern "C" void dse_cloth_set_gravity(uint32_t e, float gx, float gy, float gz) {
    World* world = GW();
    if (!world) return;
    auto* cloth = world->registry().try_get<ClothComponent>(TE(e));
    if (!cloth) return;
    cloth->gravity = glm::vec3(gx, gy, gz);
}

extern "C" void dse_cloth_pin_vertices(uint32_t e, const uint32_t* vertices, int count) {
    World* world = GW();
    if (!world) return;
    auto* cloth = world->registry().try_get<ClothComponent>(TE(e));
    if (!cloth) return;
    cloth->pinned_vertices.clear();
    if (vertices && count > 0) {
        cloth->pinned_vertices.reserve(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) cloth->pinned_vertices.push_back(vertices[i]);
    }
}

extern "C" void dse_cloth_add_sphere_collider(uint32_t e, uint32_t collider_entity, float radius) {
    World* world = GW();
    if (!world) return;
    auto* cloth = world->registry().try_get<ClothComponent>(TE(e));
    if (!cloth) return;
    ClothSphereCollider col;
    col.entity_id = collider_entity;
    col.radius = radius;
    cloth->sphere_colliders.push_back(col);
}

// ============================================================
// Fluid — SPH 流体发射器
// ============================================================

extern "C" void dse_fluid_add_emitter(uint32_t e, int shape, float emission_rate,
                                      float particle_lifetime, float emit_speed) {
    World* world = GW();
    if (!world) return;
    auto& fluid = world->registry().emplace_or_replace<FluidEmitterComponent>(TE(e));
    fluid.enabled = true;
    fluid.shape = static_cast<FluidEmitterShape>(shape);
    fluid.emission_rate = emission_rate;
    fluid.particle_lifetime = particle_lifetime;
    fluid.emit_speed = emit_speed;
}

extern "C" void dse_fluid_set_physics(uint32_t e, float viscosity, float surface_tension,
                                      float rest_density, float gas_stiffness) {
    World* world = GW();
    if (!world) return;
    auto* fluid = world->registry().try_get<FluidEmitterComponent>(TE(e));
    if (!fluid) return;
    if (!Keep(viscosity))       fluid->viscosity = viscosity;
    if (!Keep(surface_tension)) fluid->surface_tension = surface_tension;
    if (!Keep(rest_density))    fluid->rest_density = rest_density;
    if (!Keep(gas_stiffness))   fluid->gas_stiffness = gas_stiffness;
}

extern "C" void dse_fluid_set_rendering(uint32_t e, float r, float g, float b, float a,
                                        float refraction, float fresnel, float specular) {
    World* world = GW();
    if (!world) return;
    auto* fluid = world->registry().try_get<FluidEmitterComponent>(TE(e));
    if (!fluid) return;
    fluid->color = glm::vec4(r, g, b, a);
    if (!Keep(refraction)) fluid->refraction_strength = refraction;
    if (!Keep(fresnel))    fluid->fresnel_power = fresnel;
    if (!Keep(specular))   fluid->specular_intensity = specular;
}

extern "C" void dse_fluid_set_emit_direction(uint32_t e, float dx, float dy, float dz, float spread) {
    World* world = GW();
    if (!world) return;
    auto* fluid = world->registry().try_get<FluidEmitterComponent>(TE(e));
    if (!fluid) return;
    fluid->emit_direction = glm::normalize(glm::vec3(dx, dy, dz));
    if (!Keep(spread)) fluid->emit_spread = spread;
}

extern "C" void dse_fluid_set_floor(uint32_t e, float floor_y, float restitution) {
    World* world = GW();
    if (!world) return;
    auto* fluid = world->registry().try_get<FluidEmitterComponent>(TE(e));
    if (!fluid) return;
    if (!Keep(floor_y))     fluid->floor_y = floor_y;
    if (!Keep(restitution)) fluid->collision_restitution = restitution;
}

extern "C" uint32_t dse_fluid_get_particle_count(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* fluid = world->registry().try_get<FluidEmitterComponent>(TE(e));
    return fluid ? fluid->active_count : 0u;
}
