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
    char _buf[4096];
    int _count = dse_ecs_get_queryable_components(_buf, sizeof(_buf));
    lua_newtable(L);
    int _offset = 0;
    int _idx = 1;
    for (int _i = 0; _i < _count && _offset < static_cast<int>(sizeof(_buf)); ++_i) {
        const char* _name = _buf + _offset;
        lua_pushstring(L, _name);
        lua_rawseti(L, -2, _idx++);
        _offset += static_cast<int>(strlen(_name)) + 1;
    }
    return 1;
}

int L_dse_ecs_get_script_path(lua_State* L) {
    int e = static_cast<int>(luaL_checkinteger(L, 1));
    char _buf[1024];
    int _count = dse_ecs_get_script_path(e, _buf, sizeof(_buf));
    lua_newtable(L);
    int _offset = 0;
    int _idx = 1;
    for (int _i = 0; _i < _count && _offset < static_cast<int>(sizeof(_buf)); ++_i) {
        const char* _name = _buf + _offset;
        lua_pushstring(L, _name);
        lua_rawseti(L, -2, _idx++);
        _offset += static_cast<int>(strlen(_name)) + 1;
    }
    return 1;
}

int L_dse_particle_system_3d_get_state(lua_State* L) {
    int _out_active = 0;
    int _out_max_particles = 0;
    float _out_emission_rate = 0;
    uint32_t _out_texture_handle = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _out_life[2] = {0,0};
    float _out_size[2] = {0,0};
    float _out_speed[2] = {0,0};
    float _out_gravity[3] = {0,0,0};
    float _out_color[4] = {0,0,0,0};
    char _out_tex[256] = {0};
    int _out_enabled = 0;
    int _out_initialized = 0;
    dse_particle_system_3d_get_state(e, &_out_active, &_out_max_particles, &_out_emission_rate, _out_life, _out_size, _out_speed, _out_gravity, _out_color, _out_tex, sizeof(_out_tex), &_out_enabled, &_out_initialized, &_out_texture_handle);
    lua_pushinteger(L, _out_active);
    lua_pushinteger(L, _out_max_particles);
    lua_pushnumber(L, _out_emission_rate);
    lua_pushinteger(L, static_cast<lua_Integer>(_out_texture_handle));
    return 4;
}

int L_set_nav_agent(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float speed=3.5f, acceleration=8.0f, stopping_dist=0.5f, radius=0.5f, height=1.8f;
    if (lua_istable(L, 2)) { lua_getfield(L, 2, "speed"); if (!lua_isnil(L,-1)) speed=static_cast<float>(lua_tonumber(L,-1)); lua_pop(L,1); lua_getfield(L, 2, "acceleration"); if (!lua_isnil(L,-1)) acceleration=static_cast<float>(lua_tonumber(L,-1)); lua_pop(L,1); lua_getfield(L, 2, "stopping_dist"); if (!lua_isnil(L,-1)) stopping_dist=static_cast<float>(lua_tonumber(L,-1)); lua_pop(L,1); lua_getfield(L, 2, "radius"); if (!lua_isnil(L,-1)) radius=static_cast<float>(lua_tonumber(L,-1)); lua_pop(L,1); lua_getfield(L, 2, "height"); if (!lua_isnil(L,-1)) height=static_cast<float>(lua_tonumber(L,-1)); lua_pop(L,1); }
    dse_nav_agent_set(e, speed, acceleration, stopping_dist, radius, height);
    return 0;
}

int L_set_nav_destination(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    dse_nav_agent_set_destination(e, x, y, z);
    return 0;
}

int L_dse_nav_agent_arrived(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_nav_agent_arrived(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_renderer_set_texture(lua_State* L) {
    uint32_t out_handle = 0;
    int out_width = 0;
    int out_height = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* slot = luaL_checkstring(L, 2);
    const char* path = luaL_checkstring(L, 3);
    int _ret = dse_mesh_renderer_set_texture(e, slot, path, &out_handle, &out_width, &out_height);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_handle));
    lua_pushinteger(L, out_width);
    lua_pushinteger(L, out_height);
    return 4;
}

int L_dse_physics3d_get_collision_events(lua_State* L) {
    float _buf[2816];
    int _count = dse_physics3d_get_collision_events(_buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        float* _p = _buf + _i * 11;
        lua_newtable(L);
        lua_pushinteger(L, static_cast<lua_Integer>(_p[0])); lua_setfield(L, -2, "type");
        lua_pushinteger(L, static_cast<lua_Integer>(_p[1])); lua_setfield(L, -2, "entity_a");
        lua_pushinteger(L, static_cast<lua_Integer>(_p[2])); lua_setfield(L, -2, "entity_b");
        lua_pushnumber(L, _p[3]); lua_setfield(L, -2, "cx");
        lua_pushnumber(L, _p[4]); lua_setfield(L, -2, "cy");
        lua_pushnumber(L, _p[5]); lua_setfield(L, -2, "cz");
        lua_pushnumber(L, _p[6]); lua_setfield(L, -2, "nx");
        lua_pushnumber(L, _p[7]); lua_setfield(L, -2, "ny");
        lua_pushnumber(L, _p[8]); lua_setfield(L, -2, "nz");
        lua_pushnumber(L, _p[9]); lua_setfield(L, -2, "impulse");
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_physics3d_get_trigger_events(lua_State* L) {
    uint32_t _ent_buf[512];
    int _type_buf[256];
    int _count = dse_physics3d_get_trigger_events(_ent_buf, _type_buf, 256);
    lua_newtable(L);
    for (int _i = 0; _i < _count; ++_i) {
        lua_newtable(L);
        lua_pushinteger(L, static_cast<lua_Integer>(_ent_buf[_i * 2])); lua_setfield(L, -2, "trigger_entity");
        lua_pushinteger(L, static_cast<lua_Integer>(_ent_buf[_i * 2 + 1])); lua_setfield(L, -2, "other_entity");
        lua_pushinteger(L, _type_buf[_i]); lua_setfield(L, -2, "type");
        lua_rawseti(L, -2, _i + 1);
    }
    return 1;
}

int L_dse_physics2d_poll_collision_event(lua_State* L) {
    uint32_t out_other = 0;
    int out_is_trigger = 0;
    int out_is_enter = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_physics2d_poll_collision_event(e, &out_other, &out_is_trigger, &out_is_enter);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_other));
    lua_pushinteger(L, out_is_trigger);
    lua_pushinteger(L, out_is_enter);
    return 4;
}

int L_dse_physics2d_raycast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float ex = static_cast<float>(luaL_checknumber(L, 3));
    float ey = static_cast<float>(luaL_checknumber(L, 4));
    int _ret = dse_physics2d_raycast(sx, sy, ex, ey, &out_entity, out_point, out_normal);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    return 8;
}

int L_dse_terrain_get_lod(lua_State* L) {
    int out_lod = 0;
    int out_rx = 0;
    int out_rz = 0;
    int out_max_lod = 0;
    float out_lod_factor = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_terrain_get_lod(e, &out_lod, &out_rx, &out_rz, &out_max_lod, &out_lod_factor);
    lua_pushinteger(L, out_lod);
    lua_pushinteger(L, out_rx);
    lua_pushinteger(L, out_rz);
    lua_pushinteger(L, out_max_lod);
    lua_pushnumber(L, out_lod_factor);
    return 5;
}

int L_dse_terrain_load_heightmap(lua_State* L) {
    int out_w = 0;
    int out_h = 0;
    int out_ch = 0;
    int out_rx = 0;
    int out_rz = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_terrain_load_heightmap(e, path, &out_w, &out_h, &out_ch, &out_rx, &out_rz);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_w);
    lua_pushinteger(L, out_h);
    lua_pushinteger(L, out_ch);
    lua_pushinteger(L, out_rx);
    lua_pushinteger(L, out_rz);
    return 6;
}

int L_dse_terrain_set_texture(lua_State* L) {
    uint32_t out_handle = 0;
    int out_w = 0;
    int out_h = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    int _ret = dse_terrain_set_texture(e, path, &out_handle, &out_w, &out_h);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_handle));
    lua_pushinteger(L, out_w);
    lua_pushinteger(L, out_h);
    return 4;
}

int L_set_gi_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_set_gi_probe(e, 1.0f, 0.0f, 0.0f, 0.0f, 10.0f, 10.0f, 10.0f, 16, 16, 16);
    return 0;
}

int L_dse_steering_get_state(lua_State* L) {
    int out_flags = 0;
    float out_velocity[3] = {0, 0, 0};
    float out_params = 0;
    float out_targets = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_steering_get_state(e, &out_flags, out_velocity, &out_params, &out_targets);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_flags);
    lua_pushnumber(L, out_velocity[0]);
    lua_pushnumber(L, out_velocity[1]);
    lua_pushnumber(L, out_velocity[2]);
    lua_pushnumber(L, out_params);
    lua_pushnumber(L, out_targets);
    return 7;
}

int L_set_steering_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _behavior = 0; { const char* _m = luaL_checkstring(L, 2); if (strcmp(_m, "flee") == 0) _behavior = 1; else if (strcmp(_m, "arrive") == 0) _behavior = 2; else _behavior = 0; }
    float _sx = static_cast<float>(luaL_checknumber(L, 3));
    float _sy = static_cast<float>(luaL_checknumber(L, 4));
    float _sz = static_cast<float>(luaL_checknumber(L, 5));
    dse_steering_set_target(e, _behavior, _sx, _sy, _sz);
    return 0;
}

int L_dse_anim3d_get_state(lua_State* L) {
    char out_state[256] = {0};
    float out_norm = 0;
    float out_time = 0;
    float out_speed = 0;
    int out_loop = 0;
    int out_transitioning = 0;
    int out_bone_count = 0;
    int out_has_skel = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_anim3d_get_state(e, out_state, sizeof(out_state), &out_norm, &out_time, &out_speed, &out_loop, &out_transitioning, &out_bone_count, &out_has_skel);
    lua_pushinteger(L, _ret);
    lua_pushstring(L, out_state);
    lua_pushnumber(L, out_norm);
    lua_pushnumber(L, out_time);
    lua_pushnumber(L, out_speed);
    lua_pushinteger(L, out_loop);
    lua_pushinteger(L, out_transitioning);
    lua_pushinteger(L, out_bone_count);
    lua_pushinteger(L, out_has_skel);
    return 9;
}

int L_dse_anim3d_get_root_motion_delta(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_anim3d_get_root_motion_delta(e, out_xyz);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 3;
}

int L_dse_mesh_renderer_set_uvs(lua_State* L) {
    int out_attr_count = 0;
    int out_vertex_count = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    std::vector<float> data;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) data.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    int _ret = dse_mesh_renderer_set_uvs(e, data.data(), static_cast<int>(data.size()), &out_attr_count, &out_vertex_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_attr_count);
    lua_pushinteger(L, out_vertex_count);
    return 3;
}

int L_dse_mesh_renderer_set_normals(lua_State* L) {
    int out_attr_count = 0;
    int out_vertex_count = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    std::vector<float> data;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) data.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    int _ret = dse_mesh_renderer_set_normals(e, data.data(), static_cast<int>(data.size()), &out_attr_count, &out_vertex_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_attr_count);
    lua_pushinteger(L, out_vertex_count);
    return 3;
}

int L_dse_mesh_renderer_set_tangents(lua_State* L) {
    int out_attr_count = 0;
    int out_vertex_count = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    std::vector<float> data;
    if (lua_istable(L, 2)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 2));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 2, _i);
            if (lua_isnumber(L, -1)) data.push_back(static_cast<float>(lua_tonumber(L, -1)));
            lua_pop(L, 1);
        }
    }
    int _ret = dse_mesh_renderer_set_tangents(e, data.data(), static_cast<int>(data.size()), &out_attr_count, &out_vertex_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_attr_count);
    lua_pushinteger(L, out_vertex_count);
    return 3;
}

int L_dse_scene_load_sub(lua_State* L) {
    int out_entity_count = 0;
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_load_sub(path, &out_entity_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_entity_count);
    return 2;
}

int L_dse_post_process_set_color(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float exposure = static_cast<float>(luaL_checknumber(L, 3));
    float gamma = static_cast<float>(luaL_checknumber(L, 4));
    int _ret = dse_post_process_set_color(e, enabled, exposure, gamma);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_post_process_get_color_state(lua_State* L) {
    int out_enabled = 0;
    int out_bloom_enabled = 0;
    float out_bloom_threshold = 0;
    float out_bloom_intensity = 0;
    int out_color_enabled = 0;
    float out_exposure = 0;
    float out_gamma = 0;
    int out_ssao_enabled = 0;
    float out_ssao_radius = 0;
    float out_ssao_bias = 0;
    int out_fxaa_enabled = 0;
    int out_vignette_enabled = 0;
    float out_vignette_intensity = 0;
    float out_vignette_radius = 0;
    float out_vignette_softness = 0;
    int out_film_grain_enabled = 0;
    float out_film_grain_intensity = 0;
    float out_film_grain_time_scale = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_post_process_get_color_state(e, &out_enabled, &out_bloom_enabled, &out_bloom_threshold, &out_bloom_intensity, &out_color_enabled, &out_exposure, &out_gamma, &out_ssao_enabled, &out_ssao_radius, &out_ssao_bias, &out_fxaa_enabled, &out_vignette_enabled, &out_vignette_intensity, &out_vignette_radius, &out_vignette_softness, &out_film_grain_enabled, &out_film_grain_intensity, &out_film_grain_time_scale);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushinteger(L, out_bloom_enabled);
    lua_pushnumber(L, out_bloom_threshold);
    lua_pushnumber(L, out_bloom_intensity);
    lua_pushinteger(L, out_color_enabled);
    lua_pushnumber(L, out_exposure);
    lua_pushnumber(L, out_gamma);
    lua_pushinteger(L, out_ssao_enabled);
    lua_pushnumber(L, out_ssao_radius);
    lua_pushnumber(L, out_ssao_bias);
    lua_pushinteger(L, out_fxaa_enabled);
    lua_pushinteger(L, out_vignette_enabled);
    lua_pushnumber(L, out_vignette_intensity);
    lua_pushnumber(L, out_vignette_radius);
    lua_pushnumber(L, out_vignette_softness);
    lua_pushinteger(L, out_film_grain_enabled);
    lua_pushnumber(L, out_film_grain_intensity);
    lua_pushnumber(L, out_film_grain_time_scale);
    return 19;
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
        {"set_nav_agent", L_set_nav_agent},
        {"set_nav_destination", L_set_nav_destination},
        {"nav_agent_arrived", L_dse_nav_agent_arrived},
        {"set_mesh_texture", L_dse_mesh_renderer_set_texture},
        {"physics_3d_get_collision_events", L_dse_physics3d_get_collision_events},
        {"physics_3d_get_trigger_events", L_dse_physics3d_get_trigger_events},
        {"poll_collision_event", L_dse_physics2d_poll_collision_event},
        {"raycast_2d", L_dse_physics2d_raycast},
        {"get_terrain_lod", L_dse_terrain_get_lod},
        {"load_terrain_heightmap", L_dse_terrain_load_heightmap},
        {"set_terrain_texture", L_dse_terrain_set_texture},
        {"set_gi_probe", L_set_gi_probe},
        {"get_steering_state", L_dse_steering_get_state},
        {"set_steering_target", L_set_steering_target},
        {"get_animator_3d_state", L_dse_anim3d_get_state},
        {"get_animator_3d_root_motion_delta", L_dse_anim3d_get_root_motion_delta},
        {"set_mesh_uvs", L_dse_mesh_renderer_set_uvs},
        {"set_mesh_normals", L_dse_mesh_renderer_set_normals},
        {"set_mesh_tangents", L_dse_mesh_renderer_set_tangents},
        {"load_sub_scene", L_dse_scene_load_sub},
        {"set_post_process_color", L_dse_post_process_set_color},
        {"get_post_process_state", L_dse_post_process_get_color_state},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
