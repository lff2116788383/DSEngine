/**
 * @file lua_binding_free_gap.gen.cpp
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

int L_dse_mesh_renderer_set_skeleton(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t skeleton_entity = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_mesh_renderer_set_skeleton(e, skeleton_entity);
    return 0;
}

int L_dse_mesh_renderer_get_skeleton(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t _ret = dse_mesh_renderer_get_skeleton(e);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_ai_lod_get_config(lua_State* L) {
    float out_near_dist = 0;
    float out_far_dist = 0;
    int out_max_level = 0;
    dse_ai_lod_get_config(&out_near_dist, &out_far_dist, &out_max_level);
    lua_pushnumber(L, out_near_dist);
    lua_pushnumber(L, out_far_dist);
    lua_pushinteger(L, out_max_level);
    return 3;
}

int L_dse_anim3d_get_root_motion_delta(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_anim3d_get_root_motion_delta(e, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_bone_attach_get_world_pos(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t target = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* bone_name = luaL_checkstring(L, 2);
    int _ret = dse_bone_attach_get_world_pos(target, bone_name, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_morph_simple_get_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float _ret = dse_morph_simple_get_weight(e, name);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_morph_simple_get_weight_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float _ret = dse_morph_simple_get_weight_index(e, idx);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_morph_simple_set_weight(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_simple_set_weight(e, name, w);
    return 0;
}

int L_dse_morph_simple_set_weight_index(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int idx = static_cast<int>(luaL_checkinteger(L, 2));
    float w = static_cast<float>(luaL_checknumber(L, 3));
    dse_morph_simple_set_weight_index(e, idx, w);
    return 0;
}

int L_dse_app_get_fps(lua_State* L) {
    float _ret = dse_app_get_fps();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_app_get_frame_time_ms(lua_State* L) {
    float _ret = dse_app_get_frame_time_ms();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_atlas_get_entry_uv(lua_State* L) {
    float out_uv[4] = {0, 0, 0, 0};
    int atlas = static_cast<int>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    dse_atlas_get_entry_uv(atlas, name, out_uv);
    lua_pushnumber(L, out_uv[0]);
    lua_pushnumber(L, out_uv[1]);
    lua_pushnumber(L, out_uv[2]);
    lua_pushnumber(L, out_uv[3]);
    return 4;
}

int L_dse_sprite_sheet_get_frame_uv(lua_State* L) {
    float out_uv[4] = {0, 0, 0, 0};
    int sheet = static_cast<int>(luaL_checkinteger(L, 1));
    int frame = static_cast<int>(luaL_checkinteger(L, 2));
    dse_sprite_sheet_get_frame_uv(sheet, frame, out_uv);
    lua_pushnumber(L, out_uv[0]);
    lua_pushnumber(L, out_uv[1]);
    lua_pushnumber(L, out_uv[2]);
    lua_pushnumber(L, out_uv[3]);
    return 4;
}

int L_dse_audio_lod_get_stats(lua_State* L) {
    int out_stats = 0;
    int _ret = dse_audio_lod_get_stats(&out_stats);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_stats);
    return 2;
}

int L_dse_camera3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fov = static_cast<float>(luaL_checknumber(L, 2));
    float near_clip = static_cast<float>(luaL_checknumber(L, 3));
    float far_clip = static_cast<float>(luaL_checknumber(L, 4));
    dse_camera3d_add(e, fov, near_clip, far_clip);
    return 0;
}

int L_dse_character_check_ground(lua_State* L) {
    float out_normal[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_character_check_ground(e, out_normal);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    return 4;
}

int L_dse_character_controller3d_get_position(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_character_controller3d_get_position(e, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_character_controller3d_move(lua_State* L) {
    float out_velocity[3] = {0, 0, 0};
    uint32_t out_flags = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dx = static_cast<float>(luaL_checknumber(L, 2));
    float dy = static_cast<float>(luaL_checknumber(L, 3));
    float dz = static_cast<float>(luaL_checknumber(L, 4));
    float min_dist = static_cast<float>(luaL_checknumber(L, 5));
    float dt = static_cast<float>(luaL_checknumber(L, 6));
    int _ret = dse_character_controller3d_move(e, dx, dy, dz, min_dist, dt, out_velocity, &out_flags);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_velocity[0]);
    lua_pushnumber(L, out_velocity[1]);
    lua_pushnumber(L, out_velocity[2]);
    lua_pushinteger(L, static_cast<lua_Integer>(out_flags));
    return 5;
}

int L_dse_physics3d_boxcast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float out_distance = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float hx = static_cast<float>(luaL_checknumber(L, 7));
    float hy = static_cast<float>(luaL_checknumber(L, 8));
    float hz = static_cast<float>(luaL_checknumber(L, 9));
    float max_dist = static_cast<float>(luaL_checknumber(L, 10));
    int _ret = dse_physics3d_boxcast(ox, oy, oz, dx, dy, dz, hx, hy, hz, max_dist, &out_entity, out_point, out_normal, &out_distance);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    lua_pushnumber(L, out_distance);
    return 9;
}

int L_dse_physics3d_get_collision_count(lua_State* L) {
    int _ret = dse_physics3d_get_collision_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_physics3d_get_trigger_count(lua_State* L) {
    int _ret = dse_physics3d_get_trigger_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_physics3d_raycast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float out_distance = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float max_dist = static_cast<float>(luaL_checknumber(L, 7));
    int _ret = dse_physics3d_raycast(ox, oy, oz, dx, dy, dz, max_dist, &out_entity, out_point, out_normal, &out_distance);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    lua_pushnumber(L, out_distance);
    return 9;
}

int L_dse_physics3d_spherecast(lua_State* L) {
    uint32_t out_entity = 0;
    float out_point[3] = {0, 0, 0};
    float out_normal[3] = {0, 0, 0};
    float out_distance = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float radius = static_cast<float>(luaL_checknumber(L, 7));
    float max_dist = static_cast<float>(luaL_checknumber(L, 8));
    int _ret = dse_physics3d_spherecast(ox, oy, oz, dx, dy, dz, radius, max_dist, &out_entity, out_point, out_normal, &out_distance);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(out_entity));
    lua_pushnumber(L, out_point[0]);
    lua_pushnumber(L, out_point[1]);
    lua_pushnumber(L, out_point[2]);
    lua_pushnumber(L, out_normal[0]);
    lua_pushnumber(L, out_normal[1]);
    lua_pushnumber(L, out_normal[2]);
    lua_pushnumber(L, out_distance);
    return 9;
}

int L_dse_physics_lod_get_stats(lua_State* L) {
    int out_stats = 0;
    int _ret = dse_physics_lod_get_stats(&out_stats);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_stats);
    return 2;
}

int L_dse_rigidbody3d_get_angular_velocity(lua_State* L) {
    float out_vel[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_get_angular_velocity(e, out_vel);
    lua_pushnumber(L, out_vel[0]);
    lua_pushnumber(L, out_vel[1]);
    lua_pushnumber(L, out_vel[2]);
    return 3;
}

int L_dse_rigidbody3d_get_velocity(lua_State* L) {
    float out_vel[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rigidbody3d_get_velocity(e, out_vel);
    lua_pushnumber(L, out_vel[0]);
    lua_pushnumber(L, out_vel[1]);
    lua_pushnumber(L, out_vel[2]);
    return 3;
}

int L_dse_clipmap_get_config(lua_State* L) {
    float out_cell_size = 0;
    int out_levels = 0;
    dse_clipmap_get_config(&out_cell_size, &out_levels);
    lua_pushnumber(L, out_cell_size);
    lua_pushinteger(L, out_levels);
    return 2;
}

int L_dse_clipmap_sample_height(lua_State* L) {
    float out_y = 0;
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float z = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_clipmap_sample_height(x, z, &out_y);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_y);
    return 2;
}

int L_dse_wp_cell_to_world(lua_State* L) {
    float out_x = 0;
    float out_y = 0;
    float out_z = 0;
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    float cell_size = static_cast<float>(luaL_checknumber(L, 3));
    dse_wp_cell_to_world(cx, cz, cell_size, &out_x, &out_y, &out_z);
    lua_pushnumber(L, out_x);
    lua_pushnumber(L, out_y);
    lua_pushnumber(L, out_z);
    return 3;
}

int L_dse_wp_world_to_cell(lua_State* L) {
    int out_cx = 0;
    int out_cz = 0;
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float cell_size = static_cast<float>(luaL_checknumber(L, 4));
    dse_wp_world_to_cell(x, y, z, cell_size, &out_cx, &out_cz);
    lua_pushinteger(L, out_cx);
    lua_pushinteger(L, out_cz);
    return 2;
}

int L_dse_day_night_get_sun_direction(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_day_night_get_sun_direction(e, out_xyz);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 3;
}

int L_dse_snow_cover_get(lua_State* L) {
    float out_coverage = 0;
    float out_target = 0;
    int out_enabled = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_snow_cover_get(e, &out_coverage, &out_target, &out_enabled);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_coverage);
    lua_pushnumber(L, out_target);
    lua_pushinteger(L, out_enabled);
    return 4;
}

int L_dse_dir_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_dir_light_add(e);
    return 0;
}

int L_dse_dir_light_get_shadow_params(lua_State* L) {
    int out_cast_shadow = 0;
    float out_strength = 0;
    float out_c0 = 0;
    float out_c1 = 0;
    float out_c2 = 0;
    float out_lambda = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_dir_light_get_shadow_params(e, &out_cast_shadow, &out_strength, &out_c0, &out_c1, &out_c2, &out_lambda);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_cast_shadow);
    lua_pushnumber(L, out_strength);
    lua_pushnumber(L, out_c0);
    lua_pushnumber(L, out_c1);
    lua_pushnumber(L, out_c2);
    lua_pushnumber(L, out_lambda);
    return 7;
}

int L_dse_dir_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_dir_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_dir_light_set_shadow_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int cast_shadow = static_cast<int>(luaL_checkinteger(L, 2));
    float shadow_strength = static_cast<float>(luaL_checknumber(L, 3));
    float c0 = static_cast<float>(luaL_checknumber(L, 4));
    float c1 = static_cast<float>(luaL_checknumber(L, 5));
    float c2 = static_cast<float>(luaL_checknumber(L, 6));
    float lambda = static_cast<float>(luaL_checknumber(L, 7));
    dse_dir_light_set_shadow_params(e, cast_shadow, shadow_strength, c0, c1, c2, lambda);
    return 0;
}

int L_dse_point_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_point_light_add(e);
    return 0;
}

int L_dse_point_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_point_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_rendering_add_gi_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_add_gi_probe(e);
    return 0;
}

int L_dse_rendering_add_light_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_add_light_probe(e);
    return 0;
}

int L_dse_rendering_add_reflection_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rendering_add_reflection_probe(e);
    return 0;
}

int L_dse_rendering_get_gi_probe(lua_State* L) {
    float out_gi_intensity = 0;
    float out_origin[3] = {0, 0, 0};
    float out_extent[3] = {0, 0, 0};
    int out_resolution = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_rendering_get_gi_probe(e, &out_gi_intensity, out_origin, out_extent, &out_resolution);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_gi_intensity);
    lua_pushnumber(L, out_origin[0]);
    lua_pushnumber(L, out_origin[1]);
    lua_pushnumber(L, out_origin[2]);
    lua_pushnumber(L, out_extent[0]);
    lua_pushnumber(L, out_extent[1]);
    lua_pushnumber(L, out_extent[2]);
    lua_pushinteger(L, out_resolution);
    return 9;
}

int L_dse_rendering_get_gi_probe_ex(lua_State* L) {
    int out_enabled = 0;
    float out_normal_bias[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_rendering_get_gi_probe_ex(e, &out_enabled, out_normal_bias);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushnumber(L, out_normal_bias[0]);
    lua_pushnumber(L, out_normal_bias[1]);
    lua_pushnumber(L, out_normal_bias[2]);
    return 5;
}

int L_dse_rendering_set_gi_probe(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float gi_intensity = static_cast<float>(luaL_checknumber(L, 2));
    float ox = static_cast<float>(luaL_checknumber(L, 3));
    float oy = static_cast<float>(luaL_checknumber(L, 4));
    float oz = static_cast<float>(luaL_checknumber(L, 5));
    float ex = static_cast<float>(luaL_checknumber(L, 6));
    float ey = static_cast<float>(luaL_checknumber(L, 7));
    float ez = static_cast<float>(luaL_checknumber(L, 8));
    int res_x = static_cast<int>(luaL_checkinteger(L, 9));
    int res_y = static_cast<int>(luaL_checkinteger(L, 10));
    int res_z = static_cast<int>(luaL_checkinteger(L, 11));
    dse_rendering_set_gi_probe(e, gi_intensity, ox, oy, oz, ex, ey, ez, res_x, res_y, res_z);
    return 0;
}

int L_dse_rendering_set_gi_probe_bias(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float normal_bias = static_cast<float>(luaL_checknumber(L, 2));
    float hysteresis = static_cast<float>(luaL_checknumber(L, 3));
    dse_rendering_set_gi_probe_bias(e, normal_bias, hysteresis);
    return 0;
}

int L_dse_sky_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_sky_light_add(e);
    return 0;
}

int L_dse_sky_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_sky_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_spot_light_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spot_light_add(e);
    return 0;
}

int L_dse_spot_light_has(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_spot_light_has(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_dssl_get_color(lua_State* L) {
    float out_rgba[4] = {0, 0, 0, 0};
    uint32_t instance = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* name = luaL_checkstring(L, 2);
    dse_dssl_get_color(instance, name, out_rgba);
    lua_pushnumber(L, out_rgba[0]);
    lua_pushnumber(L, out_rgba[1]);
    lua_pushnumber(L, out_rgba[2]);
    lua_pushnumber(L, out_rgba[3]);
    return 4;
}

int L_dse_ecs_get_local_aabb(lua_State* L) {
    float out_min_max[6] = {0, 0, 0, 0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ecs_get_local_aabb(e, out_min_max);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_min_max[0]);
    lua_pushnumber(L, out_min_max[1]);
    lua_pushnumber(L, out_min_max[2]);
    lua_pushnumber(L, out_min_max[3]);
    lua_pushnumber(L, out_min_max[4]);
    lua_pushnumber(L, out_min_max[5]);
    return 7;
}

int L_dse_ecs_get_world_aabb(lua_State* L) {
    float out_min_max[6] = {0, 0, 0, 0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_ecs_get_world_aabb(e, out_min_max);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_min_max[0]);
    lua_pushnumber(L, out_min_max[1]);
    lua_pushnumber(L, out_min_max[2]);
    lua_pushnumber(L, out_min_max[3]);
    lua_pushnumber(L, out_min_max[4]);
    lua_pushnumber(L, out_min_max[5]);
    return 7;
}

int L_dse_entity_valid(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_entity_valid(e);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_http_send(lua_State* L) {
    const char* method = luaL_checkstring(L, 1);
    const char* url = luaL_checkstring(L, 2);
    const char* body = luaL_checkstring(L, 3);
    const char* headers_json = luaL_checkstring(L, 4);
    int timeout_sec = static_cast<int>(luaL_checkinteger(L, 5));
    int verify_peer = static_cast<int>(luaL_checkinteger(L, 6));
    const char* ca_file = luaL_checkstring(L, 7);
    uint32_t _ret = dse_http_send(method, url, body, headers_json, timeout_sec, verify_peer, ca_file);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_input_get_touch(lua_State* L) {
    float out_x = 0;
    float out_y = 0;
    int out_phase = 0;
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    int _ret = dse_input_get_touch(index, &out_x, &out_y, &out_phase);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_x);
    lua_pushnumber(L, out_y);
    lua_pushinteger(L, out_phase);
    return 4;
}

int L_dse_mesh_renderer_set_material_from_dmat(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* dmat_path = luaL_checkstring(L, 2);
    uint32_t material_index = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    int _ret = dse_mesh_renderer_set_material_from_dmat(e, dmat_path, material_index);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_mesh_renderer_set_material_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float metallic = static_cast<float>(luaL_checknumber(L, 2));
    float roughness = static_cast<float>(luaL_checknumber(L, 3));
    float ao = static_cast<float>(luaL_checknumber(L, 4));
    float er = static_cast<float>(luaL_checknumber(L, 5));
    float eg = static_cast<float>(luaL_checknumber(L, 6));
    float eb = static_cast<float>(luaL_checknumber(L, 7));
    float normal_strength = static_cast<float>(luaL_checknumber(L, 8));
    int receive_shadow = static_cast<int>(luaL_checkinteger(L, 9));
    int double_sided = static_cast<int>(luaL_checkinteger(L, 10));
    float cr = static_cast<float>(luaL_checknumber(L, 11));
    float cg = static_cast<float>(luaL_checknumber(L, 12));
    float cb = static_cast<float>(luaL_checknumber(L, 13));
    float ca = static_cast<float>(luaL_checknumber(L, 14));
    dse_mesh_renderer_set_material_params(e, metallic, roughness, ao, er, eg, eb, normal_strength, receive_shadow, double_sided, cr, cg, cb, ca);
    return 0;
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

int L_dse_meshlet_cull_stats(lua_State* L) {
    int out_total = 0;
    int out_visible = 0;
    int out_meshes = 0;
    int out_instances = 0;
    uint32_t cull_handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_cull_stats(cull_handle, &out_total, &out_visible, &out_meshes, &out_instances);
    lua_pushinteger(L, out_total);
    lua_pushinteger(L, out_visible);
    lua_pushinteger(L, out_meshes);
    lua_pushinteger(L, out_instances);
    return 4;
}

int L_dse_meshlet_get_info(lua_State* L) {
    int out_meshlets = 0;
    int out_vertices = 0;
    int out_indices = 0;
    int out_meshlet_vertices = 0;
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_meshlet_get_info(handle, &out_meshlets, &out_vertices, &out_indices, &out_meshlet_vertices);
    lua_pushinteger(L, out_meshlets);
    lua_pushinteger(L, out_vertices);
    lua_pushinteger(L, out_indices);
    lua_pushinteger(L, out_meshlet_vertices);
    return 4;
}

int L_dse_nav_agent_get(lua_State* L) {
    float out_params = 0;
    int out_flags = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_nav_agent_get(e, &out_params, &out_flags);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_params);
    lua_pushinteger(L, out_flags);
    return 3;
}

int L_dse_nav_agent_get_destination(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_nav_agent_get_destination(e, out_xyz);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 3;
}

int L_dse_nav_find_nearest(lua_State* L) {
    float out_xyz[3] = {0, 0, 0};
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    int _ret = dse_nav_find_nearest(x, y, z, out_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_xyz[0]);
    lua_pushnumber(L, out_xyz[1]);
    lua_pushnumber(L, out_xyz[2]);
    return 4;
}

int L_dse_nav_raycast(lua_State* L) {
    float out_hit_xyz[3] = {0, 0, 0};
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    float sz = static_cast<float>(luaL_checknumber(L, 3));
    float ex = static_cast<float>(luaL_checknumber(L, 4));
    float ey = static_cast<float>(luaL_checknumber(L, 5));
    float ez = static_cast<float>(luaL_checknumber(L, 6));
    int _ret = dse_nav_raycast(sx, sy, sz, ex, ey, ez, out_hit_xyz);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_hit_xyz[0]);
    lua_pushnumber(L, out_hit_xyz[1]);
    lua_pushnumber(L, out_hit_xyz[2]);
    return 4;
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

int L_dse_post_process_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_add(e);
    return 0;
}

int L_dse_post_process_get_state(lua_State* L) {
    int out_enabled = 0;
    int out_bloom = 0;
    int out_ssao = 0;
    int out_ssr = 0;
    int out_fxaa = 0;
    int out_dof = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_post_process_get_state(e, &out_enabled, &out_bloom, &out_ssao, &out_ssr, &out_fxaa, &out_dof);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushinteger(L, out_bloom);
    lua_pushinteger(L, out_ssao);
    lua_pushinteger(L, out_ssr);
    lua_pushinteger(L, out_fxaa);
    lua_pushinteger(L, out_dof);
    return 7;
}

int L_dse_render_screen_to_world_ray(lua_State* L) {
    float out_origin[3] = {0, 0, 0};
    float out_dir[3] = {0, 0, 0};
    float sx = static_cast<float>(luaL_checknumber(L, 1));
    float sy = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_render_screen_to_world_ray(sx, sy, out_origin, out_dir);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_origin[0]);
    lua_pushnumber(L, out_origin[1]);
    lua_pushnumber(L, out_origin[2]);
    lua_pushnumber(L, out_dir[0]);
    lua_pushnumber(L, out_dir[1]);
    lua_pushnumber(L, out_dir[2]);
    return 7;
}

int L_dse_render_world_to_screen(lua_State* L) {
    float out_sx = 0;
    float out_sy = 0;
    float wx = static_cast<float>(luaL_checknumber(L, 1));
    float wy = static_cast<float>(luaL_checknumber(L, 2));
    float wz = static_cast<float>(luaL_checknumber(L, 3));
    int _ret = dse_render_world_to_screen(wx, wy, wz, &out_sx, &out_sy);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_sx);
    lua_pushnumber(L, out_sy);
    return 3;
}

int L_dse_scene_instantiate_prefab(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    int use_pos = static_cast<int>(luaL_checkinteger(L, 5));
    uint32_t _ret = dse_scene_instantiate_prefab(path, x, y, z, use_pos);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_scene_load_sub(lua_State* L) {
    int out_entity_count = 0;
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_scene_load_sub(path, &out_entity_count);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_entity_count);
    return 2;
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

int L_dse_steering_set_target(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int behavior = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    int _ret = dse_steering_set_target(e, behavior, x, y, z);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_terrain_get_height(lua_State* L) {
    float out_y = 0;
    float world_x = static_cast<float>(luaL_checknumber(L, 1));
    float world_z = static_cast<float>(luaL_checknumber(L, 2));
    int _ret = dse_terrain_get_height(world_x, world_z, &out_y);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, out_y);
    return 2;
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

int L_dse_terrain_set_params(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int res_x = static_cast<int>(luaL_checkinteger(L, 2));
    int res_z = static_cast<int>(luaL_checkinteger(L, 3));
    int max_lod = static_cast<int>(luaL_checkinteger(L, 4));
    float lod_factor = static_cast<float>(luaL_checknumber(L, 5));
    int use_dynamic_lod = static_cast<int>(luaL_checkinteger(L, 6));
    dse_terrain_set_params(e, res_x, res_z, max_lod, lod_factor, use_dynamic_lod);
    return 0;
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

int L_dse_water_get(lua_State* L) {
    int out_enabled = 0;
    float out_water_level[3] = {0, 0, 0};
    float out_deep_rgb[4] = {0, 0, 0, 0};
    float out_shallow_rgb[4] = {0, 0, 0, 0};
    float out_max_depth = 0;
    float out_transparency = 0;
    float out_wave = 0;
    float out_wdir[3] = {0, 0, 0};
    float out_refraction = 0;
    float out_reflection = 0;
    float out_spec_power = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_water_get(e, &out_enabled, out_water_level, out_deep_rgb, out_shallow_rgb, &out_max_depth, &out_transparency, &out_wave, out_wdir, &out_refraction, &out_reflection, &out_spec_power);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, out_enabled);
    lua_pushnumber(L, out_water_level[0]);
    lua_pushnumber(L, out_water_level[1]);
    lua_pushnumber(L, out_water_level[2]);
    lua_pushnumber(L, out_deep_rgb[0]);
    lua_pushnumber(L, out_deep_rgb[1]);
    lua_pushnumber(L, out_deep_rgb[2]);
    lua_pushnumber(L, out_deep_rgb[3]);
    lua_pushnumber(L, out_shallow_rgb[0]);
    lua_pushnumber(L, out_shallow_rgb[1]);
    lua_pushnumber(L, out_shallow_rgb[2]);
    lua_pushnumber(L, out_shallow_rgb[3]);
    lua_pushnumber(L, out_max_depth);
    lua_pushnumber(L, out_transparency);
    lua_pushnumber(L, out_wave);
    lua_pushnumber(L, out_wdir[0]);
    lua_pushnumber(L, out_wdir[1]);
    lua_pushnumber(L, out_wdir[2]);
    lua_pushnumber(L, out_refraction);
    lua_pushnumber(L, out_reflection);
    lua_pushnumber(L, out_spec_power);
    return 22;
}

int L_dse_water_set(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    float water_level = static_cast<float>(luaL_checknumber(L, 3));
    float dr = static_cast<float>(luaL_checknumber(L, 4));
    float dg = static_cast<float>(luaL_checknumber(L, 5));
    float db = static_cast<float>(luaL_checknumber(L, 6));
    float sr = static_cast<float>(luaL_checknumber(L, 7));
    float sg = static_cast<float>(luaL_checknumber(L, 8));
    float sb = static_cast<float>(luaL_checknumber(L, 9));
    float max_depth = static_cast<float>(luaL_checknumber(L, 10));
    float transparency = static_cast<float>(luaL_checknumber(L, 11));
    float wave_amp = static_cast<float>(luaL_checknumber(L, 12));
    float wave_freq = static_cast<float>(luaL_checknumber(L, 13));
    float wave_speed = static_cast<float>(luaL_checknumber(L, 14));
    float wdir_x = static_cast<float>(luaL_checknumber(L, 15));
    float wdir_y = static_cast<float>(luaL_checknumber(L, 16));
    float refraction = static_cast<float>(luaL_checknumber(L, 17));
    float reflection = static_cast<float>(luaL_checknumber(L, 18));
    float spec_power = static_cast<float>(luaL_checknumber(L, 19));
    float caustic_int = static_cast<float>(luaL_checknumber(L, 20));
    float caustic_scale = static_cast<float>(luaL_checknumber(L, 21));
    float foam_int = static_cast<float>(luaL_checknumber(L, 22));
    float foam_threshold = static_cast<float>(luaL_checknumber(L, 23));
    float ufog_density = static_cast<float>(luaL_checknumber(L, 24));
    float ufog_r = static_cast<float>(luaL_checknumber(L, 25));
    float ufog_g = static_cast<float>(luaL_checknumber(L, 26));
    float ufog_b = static_cast<float>(luaL_checknumber(L, 27));
    dse_water_set(e, enabled, water_level, dr, dg, db, sr, sg, sb, max_depth, transparency, wave_amp, wave_freq, wave_speed, wdir_x, wdir_y, refraction, reflection, spec_power, caustic_int, caustic_scale, foam_int, foam_threshold, ufog_density, ufog_r, ufog_g, ufog_b);
    return 0;
}

int L_dse_ui_get_scroll_offset(lua_State* L) {
    float out_x = 0;
    float out_y = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_get_scroll_offset(e, &out_x, &out_y);
    lua_pushnumber(L, out_x);
    lua_pushnumber(L, out_y);
    return 2;
}

int L_dse_ui_get_virtual_scroll_range(lua_State* L) {
    int out_start = 0;
    int out_end = 0;
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_ui_get_virtual_scroll_range(e, &out_start, &out_end);
    lua_pushinteger(L, out_start);
    lua_pushinteger(L, out_end);
    return 2;
}

} // namespace

void RegisterFreeGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setglobal(L, "dse");
    }
    helper::RegisterBindings(L, {
        {"mesh_renderer_set_skeleton", L_dse_mesh_renderer_set_skeleton},
        {"mesh_renderer_get_skeleton", L_dse_mesh_renderer_get_skeleton},
        {"ai_lod_get_config", L_dse_ai_lod_get_config},
        {"anim3d_get_root_motion_delta", L_dse_anim3d_get_root_motion_delta},
        {"bone_attach_get_world_pos", L_dse_bone_attach_get_world_pos},
        {"morph_simple_get_weight", L_dse_morph_simple_get_weight},
        {"morph_simple_get_weight_index", L_dse_morph_simple_get_weight_index},
        {"morph_simple_set_weight", L_dse_morph_simple_set_weight},
        {"morph_simple_set_weight_index", L_dse_morph_simple_set_weight_index},
        {"app_get_fps", L_dse_app_get_fps},
        {"app_get_frame_time_ms", L_dse_app_get_frame_time_ms},
        {"atlas_get_entry_uv", L_dse_atlas_get_entry_uv},
        {"sprite_sheet_get_frame_uv", L_dse_sprite_sheet_get_frame_uv},
        {"audio_lod_get_stats", L_dse_audio_lod_get_stats},
        {"camera3d_add", L_dse_camera3d_add},
        {"character_check_ground", L_dse_character_check_ground},
        {"character_controller3d_get_position", L_dse_character_controller3d_get_position},
        {"character_controller3d_move", L_dse_character_controller3d_move},
        {"physics3d_boxcast", L_dse_physics3d_boxcast},
        {"physics3d_get_collision_count", L_dse_physics3d_get_collision_count},
        {"physics3d_get_trigger_count", L_dse_physics3d_get_trigger_count},
        {"physics3d_raycast", L_dse_physics3d_raycast},
        {"physics3d_spherecast", L_dse_physics3d_spherecast},
        {"physics_lod_get_stats", L_dse_physics_lod_get_stats},
        {"rigidbody3d_get_angular_velocity", L_dse_rigidbody3d_get_angular_velocity},
        {"rigidbody3d_get_velocity", L_dse_rigidbody3d_get_velocity},
        {"clipmap_get_config", L_dse_clipmap_get_config},
        {"clipmap_sample_height", L_dse_clipmap_sample_height},
        {"wp_cell_to_world", L_dse_wp_cell_to_world},
        {"wp_world_to_cell", L_dse_wp_world_to_cell},
        {"day_night_get_sun_direction", L_dse_day_night_get_sun_direction},
        {"snow_cover_get", L_dse_snow_cover_get},
        {"dir_light_add", L_dse_dir_light_add},
        {"dir_light_get_shadow_params", L_dse_dir_light_get_shadow_params},
        {"dir_light_has", L_dse_dir_light_has},
        {"dir_light_set_shadow_params", L_dse_dir_light_set_shadow_params},
        {"point_light_add", L_dse_point_light_add},
        {"point_light_has", L_dse_point_light_has},
        {"rendering_add_gi_probe", L_dse_rendering_add_gi_probe},
        {"rendering_add_light_probe", L_dse_rendering_add_light_probe},
        {"rendering_add_reflection_probe", L_dse_rendering_add_reflection_probe},
        {"rendering_get_gi_probe", L_dse_rendering_get_gi_probe},
        {"rendering_get_gi_probe_ex", L_dse_rendering_get_gi_probe_ex},
        {"rendering_set_gi_probe", L_dse_rendering_set_gi_probe},
        {"rendering_set_gi_probe_bias", L_dse_rendering_set_gi_probe_bias},
        {"sky_light_add", L_dse_sky_light_add},
        {"sky_light_has", L_dse_sky_light_has},
        {"spot_light_add", L_dse_spot_light_add},
        {"spot_light_has", L_dse_spot_light_has},
        {"dssl_get_color", L_dse_dssl_get_color},
        {"ecs_get_local_aabb", L_dse_ecs_get_local_aabb},
        {"ecs_get_world_aabb", L_dse_ecs_get_world_aabb},
        {"entity_valid", L_dse_entity_valid},
        {"http_send", L_dse_http_send},
        {"input_get_touch", L_dse_input_get_touch},
        {"mesh_renderer_set_material_from_dmat", L_dse_mesh_renderer_set_material_from_dmat},
        {"mesh_renderer_set_material_params", L_dse_mesh_renderer_set_material_params},
        {"mesh_renderer_set_texture", L_dse_mesh_renderer_set_texture},
        {"meshlet_cull_stats", L_dse_meshlet_cull_stats},
        {"meshlet_get_info", L_dse_meshlet_get_info},
        {"nav_agent_get", L_dse_nav_agent_get},
        {"nav_agent_get_destination", L_dse_nav_agent_get_destination},
        {"nav_find_nearest", L_dse_nav_find_nearest},
        {"nav_raycast", L_dse_nav_raycast},
        {"physics2d_poll_collision_event", L_dse_physics2d_poll_collision_event},
        {"physics2d_raycast", L_dse_physics2d_raycast},
        {"post_process_add", L_dse_post_process_add},
        {"post_process_get_state", L_dse_post_process_get_state},
        {"render_screen_to_world_ray", L_dse_render_screen_to_world_ray},
        {"render_world_to_screen", L_dse_render_world_to_screen},
        {"scene_instantiate_prefab", L_dse_scene_instantiate_prefab},
        {"scene_load_sub", L_dse_scene_load_sub},
        {"steering_get_state", L_dse_steering_get_state},
        {"steering_set_target", L_dse_steering_set_target},
        {"terrain_get_height", L_dse_terrain_get_height},
        {"terrain_get_lod", L_dse_terrain_get_lod},
        {"terrain_load_heightmap", L_dse_terrain_load_heightmap},
        {"terrain_set_params", L_dse_terrain_set_params},
        {"terrain_set_texture", L_dse_terrain_set_texture},
        {"water_get", L_dse_water_get},
        {"water_set", L_dse_water_set},
        {"ui_get_scroll_offset", L_dse_ui_get_scroll_offset},
        {"ui_get_virtual_scroll_range", L_dse_ui_get_virtual_scroll_range},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
