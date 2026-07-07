/**
 * @file lua_binding_free_ecs_particles.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_particle_system_3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int max_particles = static_cast<int>(luaL_checkinteger(L, 2));
    float emission_rate = static_cast<float>(luaL_checknumber(L, 3));
    dse_particle_system_3d_add(e, max_particles, emission_rate);
    return 0;
}

int L_dse_particle_system_3d_set_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float life_min = static_cast<float>(luaL_checknumber(L, 2));
    float life_max = static_cast<float>(luaL_checknumber(L, 3));
    float size_min = static_cast<float>(luaL_checknumber(L, 4));
    float size_max = static_cast<float>(luaL_checknumber(L, 5));
    float speed_min = static_cast<float>(luaL_checknumber(L, 6));
    float speed_max = static_cast<float>(luaL_checknumber(L, 7));
    float r = static_cast<float>(luaL_checknumber(L, 8));
    float g = static_cast<float>(luaL_checknumber(L, 9));
    float b = static_cast<float>(luaL_checknumber(L, 10));
    float a = static_cast<float>(luaL_checknumber(L, 11));
    float gx = static_cast<float>(luaL_checknumber(L, 12));
    float gy = static_cast<float>(luaL_checknumber(L, 13));
    float gz = static_cast<float>(luaL_checknumber(L, 14));
    const char* texture_path = luaL_checkstring(L, 15);
    dse_particle_system_3d_set_params(e, life_min, life_max, size_min, size_max, speed_min, speed_max, r, g, b, a, gx, gy, gz, texture_path);
    return 0;
}

int L_dse_particle_emitter_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t texture_handle = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    int max_particles = static_cast<int>(luaL_checkinteger(L, 3));
    float emit_rate = static_cast<float>(luaL_checknumber(L, 4));
    dse_particle_emitter_add(e, texture_handle, max_particles, emit_rate);
    return 0;
}

int L_dse_particle_set_density(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float emit_rate_scale = static_cast<float>(luaL_checknumber(L, 2));
    dse_particle_set_density(e, emit_rate_scale);
    return 0;
}

int L_dse_particle_burst(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int count = static_cast<int>(luaL_checkinteger(L, 2));
    dse_particle_burst(e, count);
    return 0;
}

int L_dse_gameplay_tuning_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_gameplay_tuning_add(e);
    return 0;
}

int L_dse_gameplay_tuning_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float leaf_min_distance = static_cast<float>(luaL_checknumber(L, 2));
    float leaf_move_left = static_cast<float>(luaL_checknumber(L, 3));
    float leaf_move_right = static_cast<float>(luaL_checknumber(L, 4));
    float jump_speed_scale = static_cast<float>(luaL_checknumber(L, 5));
    float jump_speed_max = static_cast<float>(luaL_checknumber(L, 6));
    float camera_follow_damping = static_cast<float>(luaL_checknumber(L, 7));
    dse_gameplay_tuning_set(e, leaf_min_distance, leaf_move_left, leaf_move_right, jump_speed_scale, jump_speed_max, camera_follow_damping);
    return 0;
}

int L_dse_particle_set_random(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float vmin_x = static_cast<float>(luaL_checknumber(L, 2));
    float vmin_y = static_cast<float>(luaL_checknumber(L, 3));
    float vmin_z = static_cast<float>(luaL_checknumber(L, 4));
    float vmax_x = static_cast<float>(luaL_checknumber(L, 5));
    float vmax_y = static_cast<float>(luaL_checknumber(L, 6));
    float vmax_z = static_cast<float>(luaL_checknumber(L, 7));
    float life_min = static_cast<float>(luaL_checknumber(L, 8));
    float life_max = static_cast<float>(luaL_checknumber(L, 9));
    float size_min = static_cast<float>(luaL_checknumber(L, 10));
    float size_max = static_cast<float>(luaL_checknumber(L, 11));
    dse_particle_set_random(e, vmin_x, vmin_y, vmin_z, vmax_x, vmax_y, vmax_z, life_min, life_max, size_min, size_max);
    return 0;
}

int L_dse_particle_set_size_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float start_value = static_cast<float>(luaL_checknumber(L, 3));
    float end_value = static_cast<float>(luaL_checknumber(L, 4));
    dse_particle_set_size_curve(e, enabled, start_value, end_value);
    return 0;
}

int L_dse_particle_set_alpha_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float start_value = static_cast<float>(luaL_checknumber(L, 3));
    float end_value = static_cast<float>(luaL_checknumber(L, 4));
    dse_particle_set_alpha_curve(e, enabled, start_value, end_value);
    return 0;
}

int L_dse_particle_set_speed_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float start_value = static_cast<float>(luaL_checknumber(L, 3));
    float end_value = static_cast<float>(luaL_checknumber(L, 4));
    dse_particle_set_speed_curve(e, enabled, start_value, end_value);
    return 0;
}

int L_dse_particle_set_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gx = static_cast<float>(luaL_checknumber(L, 2));
    float gy = static_cast<float>(luaL_checknumber(L, 3));
    float gz = static_cast<float>(luaL_checknumber(L, 4));
    dse_particle_set_gravity(e, gx, gy, gz);
    return 0;
}

int L_dse_particle_set_collision(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    int mode = static_cast<int>(luaL_checkinteger(L, 3));
    float bounce = static_cast<float>(luaL_checknumber(L, 4));
    float friction = static_cast<float>(luaL_checknumber(L, 5));
    float life_loss = static_cast<float>(luaL_checknumber(L, 6));
    float ground_y = static_cast<float>(luaL_checknumber(L, 7));
    dse_particle_set_collision(e, enabled, mode, bounce, friction, life_loss, ground_y);
    return 0;
}

int L_dse_particle_set_color_curve(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float end_r = static_cast<float>(luaL_checknumber(L, 3));
    float end_g = static_cast<float>(luaL_checknumber(L, 4));
    float end_b = static_cast<float>(luaL_checknumber(L, 5));
    float end_a = static_cast<float>(luaL_checknumber(L, 6));
    dse_particle_set_color_curve(e, enabled, end_r, end_g, end_b, end_a);
    return 0;
}

int L_dse_particle_set_rotation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float rotation_min = static_cast<float>(luaL_checknumber(L, 2));
    float rotation_max = static_cast<float>(luaL_checknumber(L, 3));
    float angular_velocity_min = static_cast<float>(luaL_checknumber(L, 4));
    float angular_velocity_max = static_cast<float>(luaL_checknumber(L, 5));
    dse_particle_set_rotation(e, rotation_min, rotation_max, angular_velocity_min, angular_velocity_max);
    return 0;
}

} // namespace

void RegisterEcsParticlesBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"add_particle_system_3d", L_dse_particle_system_3d_add},
        {"set_particle_system_3d_params", L_dse_particle_system_3d_set_params},
        {"add_particle_emitter", L_dse_particle_emitter_add},
        {"set_particle_density", L_dse_particle_set_density},
        {"particle_burst", L_dse_particle_burst},
        {"add_gameplay_tuning", L_dse_gameplay_tuning_add},
        {"set_gameplay_tuning", L_dse_gameplay_tuning_set},
        {"set_particle_random", L_dse_particle_set_random},
        {"set_particle_size_curve", L_dse_particle_set_size_curve},
        {"set_particle_alpha_curve", L_dse_particle_set_alpha_curve},
        {"set_particle_speed_curve", L_dse_particle_set_speed_curve},
        {"set_particle_gravity", L_dse_particle_set_gravity},
        {"set_particle_collision", L_dse_particle_set_collision},
        {"set_particle_color_curve", L_dse_particle_set_color_curve},
        {"set_particle_rotation", L_dse_particle_set_rotation},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
