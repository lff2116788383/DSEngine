/**
 * @file lua_binding_free_ecs_gap.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_transform_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float sx = static_cast<float>(luaL_checknumber(L, 5));
    float sy = static_cast<float>(luaL_checknumber(L, 6));
    float sz = static_cast<float>(luaL_checknumber(L, 7));
    dse_transform_add(e, x, y, z, sx, sy, sz);
    return 0;
}

int L_dse_anim3d_get_blend_param(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_anim3d_get_blend_param(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_anim3d_set_blend_param(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    dse_anim3d_set_blend_param(e, value);
    return 0;
}

int L_dse_anim3d_get_layer_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float _ret = dse_anim3d_get_layer_weight(e, layer);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_anim3d_set_layer_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float weight = static_cast<float>(luaL_checknumber(L, 3));
    dse_anim3d_set_layer_weight(e, layer, weight);
    return 0;
}

int L_dse_decal_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t albedo_texture = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_decal_add(e, albedo_texture);
    return 0;
}

int L_dse_decal_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    float a = static_cast<float>(luaL_checknumber(L, 5));
    float angle_fade = static_cast<float>(luaL_checknumber(L, 6));
    dse_decal_set(e, r, g, b, a, angle_fade);
    return 0;
}

int L_dse_mesh_renderer_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* mesh_path = luaL_checkstring(L, 2);
    dse_mesh_renderer_add(e, mesh_path);
    return 0;
}

int L_dse_mesh_set_depth_state(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int depth_test = static_cast<int>(luaL_checkinteger(L, 2));
    int depth_write = static_cast<int>(luaL_checkinteger(L, 3));
    dse_mesh_set_depth_state(e, depth_test, depth_write);
    return 0;
}

int L_dse_mesh_set_emissive(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    dse_mesh_set_emissive(e, r, g, b);
    return 0;
}

int L_dse_mesh_set_material(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* material_path = luaL_checkstring(L, 2);
    dse_mesh_set_material(e, material_path);
    return 0;
}

int L_dse_mesh_set_material_scalar(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    dse_mesh_set_material_scalar(e, param_name, value);
    return 0;
}

int L_dse_mesh_set_texture_handle(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* slot = luaL_checkstring(L, 2);
    uint32_t texture_handle = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_mesh_set_texture_handle(e, slot, texture_handle);
    return 0;
}

int L_dse_rigidbody3d_set_kinematic(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int kinematic = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rigidbody3d_set_kinematic(e, kinematic);
    return 0;
}

int L_dse_rigidbody3d_get_linear_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_rigidbody3d_get_linear_damping(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_rigidbody3d_set_linear_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float damping = static_cast<float>(luaL_checknumber(L, 2));
    dse_rigidbody3d_set_linear_damping(e, damping);
    return 0;
}

int L_dse_rigidbody3d_get_angular_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_rigidbody3d_get_angular_damping(e);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_rigidbody3d_set_angular_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float damping = static_cast<float>(luaL_checknumber(L, 2));
    dse_rigidbody3d_set_angular_damping(e, damping);
    return 0;
}

int L_dse_rigidbody3d_add_force_at_position(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    float fz = static_cast<float>(luaL_checknumber(L, 4));
    float px = static_cast<float>(luaL_checknumber(L, 5));
    float py = static_cast<float>(luaL_checknumber(L, 6));
    float pz = static_cast<float>(luaL_checknumber(L, 7));
    dse_rigidbody3d_add_force_at_position(e, fx, fy, fz, px, py, pz);
    return 0;
}

int L_dse_rendering_set_light_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float influence_radius = static_cast<float>(luaL_checknumber(L, 2));
    dse_rendering_set_light_probe(e, influence_radius);
    return 0;
}

int L_dse_rendering_set_reflection_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float influence_radius = static_cast<float>(luaL_checknumber(L, 2));
    int resolution = static_cast<int>(luaL_checkinteger(L, 3));
    dse_rendering_set_reflection_probe(e, influence_radius, resolution);
    return 0;
}

int L_dse_ecs_get_queryable_components(lua_State* L) {
    const char* out = luaL_checkstring(L, 1);
    int cap = static_cast<int>(luaL_checkinteger(L, 2));
    int _ret = dse_ecs_get_queryable_components(out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_ecs_get_script_path(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    const char* out = luaL_checkstring(L, 2);
    int cap = static_cast<int>(luaL_checkinteger(L, 3));
    int _ret = dse_ecs_get_script_path(e, out, cap);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_particle_system_3d_get_state(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    int out_active = static_cast<int>(luaL_checkinteger(L, 2));
    int out_max_particles = static_cast<int>(luaL_checkinteger(L, 3));
    float out_emission_rate = static_cast<float>(luaL_checknumber(L, 4));
    float out_life = static_cast<float>(luaL_checknumber(L, 5));
    float out_size = static_cast<float>(luaL_checknumber(L, 6));
    float out_speed = static_cast<float>(luaL_checknumber(L, 7));
    float out_gravity = static_cast<float>(luaL_checknumber(L, 8));
    float out_color = static_cast<float>(luaL_checknumber(L, 9));
    const char* out_tex = luaL_checkstring(L, 10);
    int tex_cap = static_cast<int>(luaL_checkinteger(L, 11));
    int out_enabled = static_cast<int>(luaL_checkinteger(L, 12));
    int out_initialized = static_cast<int>(luaL_checkinteger(L, 13));
    int out_texture_handle = static_cast<int>(luaL_checkinteger(L, 14));
    int _ret = dse_particle_system_3d_get_state(e, out_active, out_max_particles, out_emission_rate, out_life, out_size, out_speed, out_gravity, out_color, out_tex, tex_cap, out_enabled, out_initialized, out_texture_handle);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeFn_ecs_gap(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"transform_add", L_dse_transform_add},
        {"anim3d_get_blend_param", L_dse_anim3d_get_blend_param},
        {"anim3d_set_blend_param", L_dse_anim3d_set_blend_param},
        {"anim3d_get_layer_weight", L_dse_anim3d_get_layer_weight},
        {"anim3d_set_layer_weight", L_dse_anim3d_set_layer_weight},
        {"decal_add", L_dse_decal_add},
        {"decal_set", L_dse_decal_set},
        {"mesh_renderer_add", L_dse_mesh_renderer_add},
        {"mesh_set_depth_state", L_dse_mesh_set_depth_state},
        {"mesh_set_emissive", L_dse_mesh_set_emissive},
        {"mesh_set_material", L_dse_mesh_set_material},
        {"mesh_set_material_scalar", L_dse_mesh_set_material_scalar},
        {"mesh_set_texture_handle", L_dse_mesh_set_texture_handle},
        {"rigidbody_3d_set_kinematic", L_dse_rigidbody3d_set_kinematic},
        {"rigidbody_3d_get_linear_damping", L_dse_rigidbody3d_get_linear_damping},
        {"rigidbody_3d_set_linear_damping", L_dse_rigidbody3d_set_linear_damping},
        {"rigidbody_3d_get_angular_damping", L_dse_rigidbody3d_get_angular_damping},
        {"rigidbody_3d_set_angular_damping", L_dse_rigidbody3d_set_angular_damping},
        {"rigidbody_3d_add_force_at_position", L_dse_rigidbody3d_add_force_at_position},
        {"set_light_probe", L_dse_rendering_set_light_probe},
        {"set_reflection_probe", L_dse_rendering_set_reflection_probe},
        {"ecs_get_queryable_components", L_dse_ecs_get_queryable_components},
        {"ecs_get_script_path", L_dse_ecs_get_script_path},
        {"particle_system_3d_get_state", L_dse_particle_system_3d_get_state},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
