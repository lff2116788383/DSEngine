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
#include "engine/ecs/components_3d_physics.h"
#include "engine/physics/physics3d/i_physics3d_system.h"  // DSE_HAS_PHYSICS3D

#include <glm/glm.hpp>
#include <algorithm>
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

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// Ragdoll — 布娃娃（仅设置组件标志，实际激活由 RagdollSystem 处理）
// ============================================================

extern "C" void dse_ragdoll_add(uint32_t e, float total_mass, int auto_setup,
                                float joint_stiffness, float joint_damping) {
    World* world = GW();
    if (!world) return;
    auto& rd = world->registry().emplace_or_replace<RagdollComponent>(TE(e));
    rd.total_mass = total_mass;
    rd.auto_setup = (auto_setup != 0);
    rd.joint_stiffness = joint_stiffness;
    rd.joint_damping = joint_damping;
}

extern "C" void dse_ragdoll_activate(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto* rd = world->registry().try_get<RagdollComponent>(TE(e));
    if (rd) rd->active = true;
}

extern "C" void dse_ragdoll_deactivate(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto* rd = world->registry().try_get<RagdollComponent>(TE(e));
    if (rd) rd->active = false;
}

extern "C" int dse_ragdoll_is_active(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* rd = world->registry().try_get<RagdollComponent>(TE(e));
    return (rd && rd->active) ? 1 : 0;
}

extern "C" void dse_ragdoll_set_collision_layer(uint32_t e, uint32_t layer, uint32_t mask) {
    World* world = GW();
    if (!world) return;
    auto* rd = world->registry().try_get<RagdollComponent>(TE(e));
    if (!rd) return;
    rd->collision_layer = static_cast<uint16_t>(layer);
    rd->collision_mask = static_cast<uint16_t>(mask);
}
#endif // DSE_HAS_PHYSICS3D

// ============================================================
// SoftBody — 软体（无条件编译）
// ============================================================

extern "C" void dse_softbody_add(uint32_t e, float stiffness, int iterations,
                                 float damping, float volume_stiffness) {
    World* world = GW();
    if (!world) return;
    auto& sb = world->registry().emplace_or_replace<SoftBodyComponent>(TE(e));
    sb.stiffness = stiffness;
    sb.solver_iterations = iterations;
    sb.damping = damping;
    sb.volume_stiffness = volume_stiffness;
}

extern "C" void dse_softbody_set_gravity(uint32_t e, int use_gravity, float gravity_scale) {
    World* world = GW();
    if (!world) return;
    auto* sb = world->registry().try_get<SoftBodyComponent>(TE(e));
    if (!sb) return;
    sb->use_gravity = (use_gravity != 0);
    if (!Keep(gravity_scale)) sb->gravity_scale = gravity_scale;
}

extern "C" void dse_softbody_pin_vertex(uint32_t e, int vertex_index) {
    World* world = GW();
    if (!world) return;
    auto* sb = world->registry().try_get<SoftBodyComponent>(TE(e));
    if (!sb) return;
    if (vertex_index >= 0 && vertex_index < static_cast<int>(sb->inv_masses.size())) {
        sb->inv_masses[vertex_index] = 0.0f;  // 固定点
    }
}

extern "C" uint32_t dse_softbody_get_particle_count(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* sb = world->registry().try_get<SoftBodyComponent>(TE(e));
    return sb ? static_cast<uint32_t>(sb->positions.size()) : 0u;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// Vehicle — 车辆（raycast 车辆）
// ============================================================

extern "C" void dse_vehicle_add(uint32_t e, float max_engine_force, float max_brake_force,
                                float max_steer_angle) {
    World* world = GW();
    if (!world) return;
    auto& v = world->registry().emplace_or_replace<VehicleComponent>(TE(e));
    v.max_engine_force = max_engine_force;
    v.max_brake_force = max_brake_force;
    v.max_steer_angle = max_steer_angle;
}

extern "C" void dse_vehicle_add_wheel(uint32_t e, float px, float py, float pz, float radius,
                                      int is_drive, int is_steer, float susp_stiffness,
                                      float susp_damping) {
    World* world = GW();
    if (!world) return;
    auto* v = world->registry().try_get<VehicleComponent>(TE(e));
    if (!v) return;
    VehicleWheelConfig wheel;
    wheel.position = glm::vec3(px, py, pz);
    wheel.radius = radius;
    wheel.is_drive_wheel = (is_drive != 0);
    wheel.is_steer_wheel = (is_steer != 0);
    wheel.suspension_stiffness = susp_stiffness;
    wheel.suspension_damping = susp_damping;
    v->wheels.push_back(wheel);
    v->initialized = false;  // 重新初始化
}

extern "C" void dse_vehicle_set_input(uint32_t e, float throttle, float brake, float steering) {
    World* world = GW();
    if (!world) return;
    auto* v = world->registry().try_get<VehicleComponent>(TE(e));
    if (!v) return;
    v->throttle = std::clamp(throttle, -1.0f, 1.0f);
    v->brake = std::clamp(brake, 0.0f, 1.0f);
    v->steering = std::clamp(steering, -1.0f, 1.0f);
}

extern "C" float dse_vehicle_get_speed(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* v = world->registry().try_get<VehicleComponent>(TE(e));
    return v ? v->current_speed : 0.0f;
}

extern "C" uint32_t dse_vehicle_get_wheel_count(uint32_t e) {
    World* world = GW();
    if (!world) return 0;
    const auto* v = world->registry().try_get<VehicleComponent>(TE(e));
    return v ? static_cast<uint32_t>(v->wheels.size()) : 0u;
}
#endif // DSE_HAS_PHYSICS3D

// ============================================================
// Rope — 绳索/链条（无条件编译）
// ============================================================

extern "C" void dse_rope_add(uint32_t e, int segment_count, float segment_length,
                             float damping, int iterations) {
    World* world = GW();
    if (!world) return;
    auto& rope = world->registry().emplace_or_replace<RopeComponent>(TE(e));
    rope.segment_count = segment_count;
    rope.segment_length = segment_length;
    rope.damping = damping;
    rope.solver_iterations = iterations;
}

extern "C" void dse_rope_set_anchors(uint32_t e, uint32_t anchor_a, uint32_t anchor_b,
                                     float oax, float oay, float oaz,
                                     float obx, float oby, float obz) {
    World* world = GW();
    if (!world) return;
    auto* rope = world->registry().try_get<RopeComponent>(TE(e));
    if (!rope) return;
    rope->anchor_entity_a = anchor_a;
    rope->anchor_entity_b = anchor_b;
    rope->anchor_offset_a = glm::vec3(oax, oay, oaz);
    rope->anchor_offset_b = glm::vec3(obx, oby, obz);
    rope->initialized = false;  // 重新初始化
}

// 填充 out_xyz（最多 max_points 个点，每点 3 float），返回点总数（与 max_points 无关）。
// out_xyz 为 null 或 max_points<=0 时仅返回总数，供调用方预分配缓冲。
extern "C" int dse_rope_get_positions(uint32_t e, float* out_xyz, int max_points) {
    World* world = GW();
    if (!world) return 0;
    const auto* rope = world->registry().try_get<RopeComponent>(TE(e));
    if (!rope) return 0;
    const int count = static_cast<int>(rope->positions.size());
    if (out_xyz && max_points > 0) {
        const int n = (count < max_points) ? count : max_points;
        for (int i = 0; i < n; ++i) {
            out_xyz[i * 3 + 0] = rope->positions[i].x;
            out_xyz[i * 3 + 1] = rope->positions[i].y;
            out_xyz[i * 3 + 2] = rope->positions[i].z;
        }
    }
    return count;
}

extern "C" void dse_rope_set_gravity(uint32_t e, int use_gravity, float gravity_scale) {
    World* world = GW();
    if (!world) return;
    auto* rope = world->registry().try_get<RopeComponent>(TE(e));
    if (!rope) return;
    rope->use_gravity = (use_gravity != 0);
    if (!Keep(gravity_scale)) rope->gravity_scale = gravity_scale;
}

#ifdef DSE_HAS_PHYSICS3D
// ============================================================
// Buoyancy — 浮力
// ============================================================

extern "C" void dse_buoyancy_add(uint32_t e, float water_level, float buoyancy_force,
                                 float water_drag, float angular_drag, float submerge_depth) {
    World* world = GW();
    if (!world) return;
    auto& b = world->registry().emplace_or_replace<BuoyancyComponent>(TE(e));
    b.water_level = water_level;
    b.buoyancy_force = buoyancy_force;
    b.water_drag = water_drag;
    b.water_angular_drag = angular_drag;
    b.submerge_depth = submerge_depth;
}

extern "C" void dse_buoyancy_add_sample_point(uint32_t e, float ox, float oy, float oz,
                                              float force_scale) {
    World* world = GW();
    if (!world) return;
    auto* b = world->registry().try_get<BuoyancyComponent>(TE(e));
    if (!b) return;
    BuoyancySamplePoint sp;
    sp.offset = glm::vec3(ox, oy, oz);
    sp.force_scale = force_scale;
    b->sample_points.push_back(sp);
}

extern "C" void dse_buoyancy_set_water_level(uint32_t e, float water_level) {
    World* world = GW();
    if (!world) return;
    auto* b = world->registry().try_get<BuoyancyComponent>(TE(e));
    if (b) b->water_level = water_level;
}

extern "C" float dse_buoyancy_get_submerge_ratio(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* b = world->registry().try_get<BuoyancyComponent>(TE(e));
    return b ? b->submerge_ratio : 0.0f;
}

extern "C" void dse_buoyancy_set_use_fluid(uint32_t e, int use_fluid) {
    World* world = GW();
    if (!world) return;
    auto* b = world->registry().try_get<BuoyancyComponent>(TE(e));
    if (b) b->use_fluid_system = (use_fluid != 0);
}
#endif // DSE_HAS_PHYSICS3D
