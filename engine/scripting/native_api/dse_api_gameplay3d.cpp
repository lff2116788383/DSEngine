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
#include "engine/ecs/components_3d_weather.h"
#include "engine/ecs/components_3d_snow.h"
#include "engine/ecs/components_3d_sky.h"
#include "engine/physics/physics3d/i_physics3d_system.h"  // DSE_HAS_PHYSICS3D

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <string>

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

// ============================================================
// Batch 3 — 环境子系统（无物理依赖）
// Weather / SnowCover / Atmosphere / DayNightCycle / VolumetricCloud
// ============================================================

// ---- Weather（type: 0=None,1=Rain,2=Snow；-1=保持） ----
extern "C" void dse_weather_add(uint32_t e, int type, float intensity) {
    World* world = GW();
    if (!world) return;
    auto& wc = world->registry().emplace_or_replace<WeatherComponent>(TE(e));
    wc.type = static_cast<WeatherType>(type);
    wc.intensity = intensity;
}

extern "C" void dse_weather_set(uint32_t e, int type, float intensity,
                                float wind_x, float wind_z) {
    World* world = GW();
    if (!world) return;
    auto* wc = world->registry().try_get<WeatherComponent>(TE(e));
    if (!wc) return;
    if (type >= 0) wc->type = static_cast<WeatherType>(type);
    if (!Keep(intensity)) wc->intensity = intensity;
    if (!Keep(wind_x))    wc->wind_x = wind_x;
    if (!Keep(wind_z))    wc->wind_z = wind_z;
}

extern "C" void dse_weather_set_spawn(uint32_t e, float radius, float height,
                                      int max_particles) {
    World* world = GW();
    if (!world) return;
    auto* wc = world->registry().try_get<WeatherComponent>(TE(e));
    if (!wc) return;
    if (!Keep(radius)) wc->spawn_radius = radius;
    if (!Keep(height)) wc->spawn_height = height;
    if (max_particles >= 0) wc->max_particles = max_particles;
}

// ---- SnowCover ----
extern "C" void dse_snow_cover_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<SnowCoverComponent>(TE(e));
}

extern "C" void dse_snow_cover_set(uint32_t e, float target_coverage,
                                   float accumulation_rate, float melt_rate) {
    World* world = GW();
    if (!world) return;
    auto* sc = world->registry().try_get<SnowCoverComponent>(TE(e));
    if (!sc) return;
    if (!Keep(target_coverage))   sc->target_coverage = target_coverage;
    if (!Keep(accumulation_rate)) sc->accumulation_rate = accumulation_rate;
    if (!Keep(melt_rate))         sc->melt_rate = melt_rate;
}

extern "C" void dse_snow_set_appearance(uint32_t e, float albedo_r, float albedo_g,
                                        float albedo_b, float roughness, float metallic,
                                        float threshold, float sharpness) {
    World* world = GW();
    if (!world) return;
    auto* sc = world->registry().try_get<SnowCoverComponent>(TE(e));
    if (!sc) return;
    if (!Keep(albedo_r))  sc->snow_albedo.r = albedo_r;
    if (!Keep(albedo_g))  sc->snow_albedo.g = albedo_g;
    if (!Keep(albedo_b))  sc->snow_albedo.b = albedo_b;
    if (!Keep(roughness)) sc->snow_roughness = roughness;
    if (!Keep(metallic))  sc->snow_metallic = metallic;
    if (!Keep(threshold)) sc->normal_threshold = threshold;
    if (!Keep(sharpness)) sc->edge_sharpness = sharpness;
}

// out_* 可为 null；返回 1=存在组件，0=缺失（输出置默认 0）。
extern "C" int dse_snow_cover_get(uint32_t e, float* out_coverage,
                                  float* out_target, int* out_enabled) {
    World* world = GW();
    const SnowCoverComponent* sc = world ?
        world->registry().try_get<SnowCoverComponent>(TE(e)) : nullptr;
    if (out_coverage) *out_coverage = sc ? sc->coverage : 0.0f;
    if (out_target)   *out_target   = sc ? sc->target_coverage : 0.0f;
    if (out_enabled)  *out_enabled  = (sc && sc->enabled) ? 1 : 0;
    return sc ? 1 : 0;
}

extern "C" void dse_snow_cover_set_enabled(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* sc = world->registry().try_get<SnowCoverComponent>(TE(e));
    if (sc) sc->enabled = (enabled != 0);
}

// path=null 时仅更新 tiling（NaN=保持）。
extern "C" void dse_snow_set_texture(uint32_t e, const char* path, float tiling) {
    World* world = GW();
    if (!world) return;
    auto* sc = world->registry().try_get<SnowCoverComponent>(TE(e));
    if (!sc) return;
    if (path) {
        sc->snow_texture_path = path;
        sc->snow_texture_handle = 0;  // 触发重新加载
    }
    if (!Keep(tiling)) sc->snow_tiling = tiling;
}

extern "C" void dse_snow_set_displacement(uint32_t e, float displacement_height,
                                          float deformation_strength) {
    World* world = GW();
    if (!world) return;
    auto* sc = world->registry().try_get<SnowCoverComponent>(TE(e));
    if (!sc) return;
    if (!Keep(displacement_height))  sc->displacement_height = displacement_height;
    if (!Keep(deformation_strength)) sc->deformation_strength = deformation_strength;
}

extern "C" void dse_snow_cover_remove(uint32_t e) {
    World* world = GW();
    if (!world) return;
    auto& reg = world->registry();
    if (reg.all_of<SnowCoverComponent>(TE(e))) reg.remove<SnowCoverComponent>(TE(e));
}

// ---- Atmosphere ----
extern "C" void dse_atmosphere_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<AtmosphereComponent>(TE(e));
}

extern "C" void dse_atmosphere_set_params(uint32_t e, float planet_radius,
                                          float atmosphere_height, float sun_disk_angle) {
    World* world = GW();
    if (!world) return;
    auto* atm = world->registry().try_get<AtmosphereComponent>(TE(e));
    if (!atm) return;
    if (!Keep(planet_radius))     atm->planet_radius = planet_radius;
    if (!Keep(atmosphere_height)) atm->atmosphere_height = atmosphere_height;
    if (!Keep(sun_disk_angle))    atm->sun_disk_angle = sun_disk_angle;
}

extern "C" void dse_atmosphere_set_rayleigh(uint32_t e, float coeff_r, float coeff_g,
                                            float coeff_b, float scale_height) {
    World* world = GW();
    if (!world) return;
    auto* atm = world->registry().try_get<AtmosphereComponent>(TE(e));
    if (!atm) return;
    if (!Keep(coeff_r))      atm->rayleigh_coeff.x = coeff_r;
    if (!Keep(coeff_g))      atm->rayleigh_coeff.y = coeff_g;
    if (!Keep(coeff_b))      atm->rayleigh_coeff.z = coeff_b;
    if (!Keep(scale_height)) atm->rayleigh_scale_height = scale_height;
}

extern "C" void dse_atmosphere_set_mie(uint32_t e, float coeff, float scale_height, float g) {
    World* world = GW();
    if (!world) return;
    auto* atm = world->registry().try_get<AtmosphereComponent>(TE(e));
    if (!atm) return;
    if (!Keep(coeff))        atm->mie_coeff = coeff;
    if (!Keep(scale_height)) atm->mie_scale_height = scale_height;
    if (!Keep(g))            atm->mie_g = g;
}

extern "C" void dse_atmosphere_set_sun_intensity(uint32_t e, float r, float g, float b) {
    World* world = GW();
    if (!world) return;
    auto* atm = world->registry().try_get<AtmosphereComponent>(TE(e));
    if (!atm) return;
    if (!Keep(r)) atm->sun_intensity.x = r;
    if (!Keep(g)) atm->sun_intensity.y = g;
    if (!Keep(b)) atm->sun_intensity.z = b;
}

// ---- DayNightCycle ----
extern "C" void dse_day_night_add(uint32_t e, float time_of_day, int auto_advance,
                                  float time_speed) {
    World* world = GW();
    if (!world) return;
    auto& dnc = world->registry().emplace_or_replace<DayNightCycleComponent>(TE(e));
    dnc.time_of_day = time_of_day;
    dnc.auto_advance = (auto_advance != 0);
    dnc.time_speed = time_speed;
}

extern "C" void dse_day_night_set_time(uint32_t e, float time_of_day) {
    World* world = GW();
    if (!world) return;
    auto* dnc = world->registry().try_get<DayNightCycleComponent>(TE(e));
    if (dnc) dnc->time_of_day = time_of_day;
}

extern "C" float dse_day_night_get_time(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* dnc = world->registry().try_get<DayNightCycleComponent>(TE(e));
    return dnc ? dnc->time_of_day : 0.0f;
}

extern "C" void dse_day_night_set_speed(uint32_t e, float speed) {
    World* world = GW();
    if (!world) return;
    auto* dnc = world->registry().try_get<DayNightCycleComponent>(TE(e));
    if (dnc) dnc->time_speed = speed;
}

extern "C" void dse_day_night_set_auto_advance(uint32_t e, int enabled) {
    World* world = GW();
    if (!world) return;
    auto* dnc = world->registry().try_get<DayNightCycleComponent>(TE(e));
    if (dnc) dnc->auto_advance = (enabled != 0);
}

extern "C" void dse_day_night_set_location(uint32_t e, float latitude, float longitude,
                                           int day_of_year) {
    World* world = GW();
    if (!world) return;
    auto* dnc = world->registry().try_get<DayNightCycleComponent>(TE(e));
    if (!dnc) return;
    if (!Keep(latitude))   dnc->latitude = latitude;
    if (!Keep(longitude))  dnc->longitude = longitude;
    if (day_of_year > 0)   dnc->day_of_year = day_of_year;
}

extern "C" float dse_day_night_get_sun_elevation(uint32_t e) {
    World* world = GW();
    if (!world) return 0.0f;
    const auto* dnc = world->registry().try_get<DayNightCycleComponent>(TE(e));
    return dnc ? dnc->sun_elevation_ : 0.0f;
}

// out_xyz 填充归一化太阳方向（3 float）；缺失时填默认 (0,-1,0)。
extern "C" void dse_day_night_get_sun_direction(uint32_t e, float* out_xyz) {
    if (!out_xyz) return;
    World* world = GW();
    const DayNightCycleComponent* dnc = world ?
        world->registry().try_get<DayNightCycleComponent>(TE(e)) : nullptr;
    if (dnc) {
        out_xyz[0] = dnc->sun_direction_.x;
        out_xyz[1] = dnc->sun_direction_.y;
        out_xyz[2] = dnc->sun_direction_.z;
    } else {
        out_xyz[0] = 0.0f; out_xyz[1] = -1.0f; out_xyz[2] = 0.0f;
    }
}

// ---- VolumetricCloud ----
extern "C" void dse_volumetric_cloud_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<VolumetricCloudComponent>(TE(e));
}

extern "C" void dse_cloud_set_layer(uint32_t e, float bottom, float top,
                                    float coverage, float density) {
    World* world = GW();
    if (!world) return;
    auto* vc = world->registry().try_get<VolumetricCloudComponent>(TE(e));
    if (!vc) return;
    if (!Keep(bottom))   vc->cloud_bottom = bottom;
    if (!Keep(top))      vc->cloud_top = top;
    if (!Keep(coverage)) vc->coverage = coverage;
    if (!Keep(density))  vc->density = density;
}

extern "C" void dse_cloud_set_wind(uint32_t e, float dir_x, float dir_y, float speed) {
    World* world = GW();
    if (!world) return;
    auto* vc = world->registry().try_get<VolumetricCloudComponent>(TE(e));
    if (!vc) return;
    if (!Keep(dir_x)) vc->wind_direction.x = dir_x;
    if (!Keep(dir_y)) vc->wind_direction.y = dir_y;
    if (!Keep(speed)) vc->wind_speed = speed;
}
