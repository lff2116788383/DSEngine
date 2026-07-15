/**
 * @file dse_api_particles3d.cpp
 * @brief DSEngine C ABI - Particles 3D
 * Split from dse_api_extended.cpp for maintainability.
 */

#include "engine/scripting/native_api/dse_api_internal.h"
#include "engine/ecs/components_3d_particle.h"
#include "engine/render/particles/gpu_particle_system.h"

using namespace dse;
using namespace dse_api_internal;


extern "C" void dse_particle_system_3d_add(uint32_t e, int max_particles, float emission_rate) {
    World* world = GW();
    if (!world) return;
    auto& ps = world->registry().emplace_or_replace<ParticleSystem3DComponent>(TE(e));
    ps.max_particles = max_particles;
    ps.emission_rate = emission_rate;
}

extern "C" void dse_particle_system_3d_set_params(uint32_t e,
                                                  float life_min, float life_max,
                                                  float size_min, float size_max,
                                                  float speed_min, float speed_max,
                                                  float r, float g, float b, float a,
                                                  float gx, float gy, float gz,
                                                  const char* texture_path) {
    World* world = GW();
    if (!world) return;
    auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (!ps) return;
    if (!Keep(life_min))  ps->start_life_min  = life_min;
    if (!Keep(life_max))  ps->start_life_max  = life_max;
    if (!Keep(size_min))  ps->start_size_min  = size_min;
    if (!Keep(size_max))  ps->start_size_max  = size_max;
    if (!Keep(speed_min)) ps->start_speed_min = speed_min;
    if (!Keep(speed_max)) ps->start_speed_max = speed_max;
    if (!Keep(r)) ps->start_color.r = r;
    if (!Keep(g)) ps->start_color.g = g;
    if (!Keep(b)) ps->start_color.b = b;
    if (!Keep(a)) ps->start_color.a = a;
    if (!Keep(gx)) ps->gravity.x = gx;
    if (!Keep(gy)) ps->gravity.y = gy;
    if (!Keep(gz)) ps->gravity.z = gz;
    if (texture_path) ps->texture_path = texture_path;
}

extern "C" int dse_particle_system_3d_get_state(uint32_t e, int* out_active, int* out_max_particles,
                                                float* out_emission_rate,
                                                float* out_life, float* out_size, float* out_speed,
                                                float* out_gravity, float* out_color,
                                                char* out_tex, int tex_cap,
                                                int* out_enabled, int* out_initialized,
                                                uint32_t* out_texture_handle) {
    World* world = GW();
    if (!world) return 0;
    const auto* ps = world->registry().try_get<ParticleSystem3DComponent>(TE(e));
    if (!ps) return 0;
    if (out_active) *out_active = ps->active_particle_count;
    if (out_max_particles) *out_max_particles = ps->max_particles;
    if (out_emission_rate) *out_emission_rate = ps->emission_rate;
    if (out_life)  { out_life[0] = ps->start_life_min;  out_life[1] = ps->start_life_max; }
    if (out_size)  { out_size[0] = ps->start_size_min;  out_size[1] = ps->start_size_max; }
    if (out_speed) { out_speed[0] = ps->start_speed_min; out_speed[1] = ps->start_speed_max; }
    if (out_gravity) { out_gravity[0] = ps->gravity.x; out_gravity[1] = ps->gravity.y; out_gravity[2] = ps->gravity.z; }
    if (out_color) { out_color[0] = ps->start_color.r; out_color[1] = ps->start_color.g;
                     out_color[2] = ps->start_color.b; out_color[3] = ps->start_color.a; }
    WriteStr(ps->texture_path, out_tex, tex_cap);
    if (out_enabled) *out_enabled = ps->enabled ? 1 : 0;
    if (out_initialized) *out_initialized = ps->initialized ? 1 : 0;
    if (out_texture_handle) *out_texture_handle = ps->texture_handle.raw();
    return 1;
}

extern "C" void dse_particle_emitter_add(uint32_t e, uint32_t texture_handle, int max_particles, float emit_rate) {
    World* world = GW();
    if (!world) return;
    auto& em = world->registry().emplace_or_replace<ParticleEmitterComponent>(TE(e));
    em.texture_handle = dse::render::TextureHandle::from_raw(texture_handle);
    em.max_particles = max_particles;
    em.emit_rate = emit_rate;
}

extern "C" void dse_particle_set_density(uint32_t e, float emit_rate_scale) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (pe) pe->emit_rate_scale = std::max(0.0f, emit_rate_scale);
}

extern "C" void dse_particle_burst(uint32_t e, int count) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (pe) pe->pending_burst += std::max(0, count);
}

extern "C" void dse_particle_set_random(uint32_t e,
                                        float vmin_x, float vmin_y, float vmin_z,
                                        float vmax_x, float vmax_y, float vmax_z,
                                        float life_min, float life_max,
                                        float size_min, float size_max) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    pe->use_random_params = true;
    pe->velocity_min = glm::vec3(vmin_x, vmin_y, vmin_z);
    pe->velocity_max = glm::vec3(vmax_x, vmax_y, vmax_z);
    if (!Keep(life_min)) pe->life_time_min = life_min;
    if (!Keep(life_max)) pe->life_time_max = life_max;
    if (!Keep(size_min)) pe->size_min = size_min;
    if (!Keep(size_max)) pe->size_max = size_max;
}

extern "C" void dse_particle_set_size_curve(uint32_t e, int enabled, float start_value, float end_value) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    pe->size_curve.enabled = (enabled != 0);
    if (!Keep(start_value)) pe->size_curve.start_value = start_value;
    if (!Keep(end_value))   pe->size_curve.end_value = end_value;
}

extern "C" void dse_particle_set_alpha_curve(uint32_t e, int enabled, float start_value, float end_value) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    pe->alpha_curve.enabled = (enabled != 0);
    if (!Keep(start_value)) pe->alpha_curve.start_value = start_value;
    if (!Keep(end_value))   pe->alpha_curve.end_value = end_value;
}

extern "C" void dse_particle_set_speed_curve(uint32_t e, int enabled, float start_value, float end_value) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    pe->speed_curve.enabled = (enabled != 0);
    if (!Keep(start_value)) pe->speed_curve.start_value = start_value;
    if (!Keep(end_value))   pe->speed_curve.end_value = end_value;
}

extern "C" void dse_particle_set_gravity(uint32_t e, float gx, float gy, float gz) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (pe) pe->gravity = glm::vec3(gx, gy, gz);
}

extern "C" void dse_particle_set_collision(uint32_t e, int enabled, int mode, float bounce,
                                           float friction, float life_loss, float ground_y) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    pe->enable_collision = (enabled != 0);
    if (mode >= 0) pe->collision_mode = static_cast<ParticleCollisionMode>(mode);
    if (!Keep(bounce))    pe->collision_bounce = bounce;
    if (!Keep(friction))  pe->collision_friction = friction;
    if (!Keep(life_loss)) pe->collision_life_loss = life_loss;
    if (!Keep(ground_y))  pe->ground_y = ground_y;
}

extern "C" void dse_particle_set_color_curve(uint32_t e, int enabled,
                                             float end_r, float end_g, float end_b, float end_a) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    pe->use_color_curve = (enabled != 0);
    if (!Keep(end_r)) pe->color_curve_end.r = end_r;
    if (!Keep(end_g)) pe->color_curve_end.g = end_g;
    if (!Keep(end_b)) pe->color_curve_end.b = end_b;
    if (!Keep(end_a)) pe->color_curve_end.a = end_a;
}

extern "C" void dse_particle_set_rotation(uint32_t e, float rotation_min, float rotation_max,
                                          float angular_velocity_min, float angular_velocity_max) {
    World* world = GW();
    if (!world) return;
    auto* pe = world->registry().try_get<ParticleEmitterComponent>(TE(e));
    if (!pe) return;
    if (!Keep(rotation_min)) pe->rotation_min = rotation_min;
    if (!Keep(rotation_max)) pe->rotation_max = rotation_max;
    if (!Keep(angular_velocity_min)) pe->angular_velocity_min = angular_velocity_min;
    if (!Keep(angular_velocity_max)) pe->angular_velocity_max = angular_velocity_max;
}

extern "C" void dse_gameplay_tuning_add(uint32_t e) {
    World* world = GW();
    if (!world) return;
    world->registry().emplace_or_replace<GameplayTuningComponent>(TE(e));
}

extern "C" void dse_gameplay_tuning_set(uint32_t e, float leaf_min_distance,
                                        float leaf_move_left, float leaf_move_right,
                                        float jump_speed_scale, float jump_speed_max,
                                        float camera_follow_damping) {
    World* world = GW();
    if (!world) return;
    auto* t = world->registry().try_get<GameplayTuningComponent>(TE(e));
    if (!t) return;
    if (!Keep(leaf_min_distance)) t->leaf_min_distance = leaf_min_distance;
    if (!Keep(leaf_move_left))    t->leaf_move_left = leaf_move_left;
    if (!Keep(leaf_move_right))   t->leaf_move_right = leaf_move_right;
    if (!Keep(jump_speed_scale))  t->jump_speed_scale = jump_speed_scale;
    if (!Keep(jump_speed_max))    t->jump_speed_max = jump_speed_max;
    if (!Keep(camera_follow_damping)) t->camera_follow_damping = camera_follow_damping;
}
