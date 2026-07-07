/**
 * @file lua_binding_free_ecs_gap.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/function_defs.json
 *
 * ecs_gap 组自由函数的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_transform_add(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float sx = static_cast<float>(luaL_optnumber(L, 5, 1.0));
    float sy = static_cast<float>(luaL_optnumber(L, 6, 1.0));
    float sz = static_cast<float>(luaL_optnumber(L, 7, 1.0));
    dse_transform_add(entity, x, y, z, sx, sy, sz);
    return 0;
}

int L_dse_anim3d_get_blend_param(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_anim3d_get_blend_param(entity);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_anim3d_set_blend_param(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    dse_anim3d_set_blend_param(entity, value);
    return 0;
}

int L_dse_anim3d_get_layer_weight(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float _ret = dse_anim3d_get_layer_weight(entity, layer);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_anim3d_set_layer_weight(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int layer = static_cast<int>(luaL_checkinteger(L, 2));
    float weight = static_cast<float>(luaL_checknumber(L, 3));
    dse_anim3d_set_layer_weight(entity, layer, weight);
    return 0;
}

int L_dse_character_check_ground(lua_State* L) {
    float normal[3] = {0, 0, 0};
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_character_check_ground(entity, normal);
    lua_pushinteger(L, _ret);
    lua_pushnumber(L, normal[0]);
    lua_pushnumber(L, normal[1]);
    lua_pushnumber(L, normal[2]);
    return 4;
}

int L_dse_decal_add(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t material_id = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_decal_add(entity, material_id);
    return 0;
}

int L_dse_decal_set(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float sx = static_cast<float>(luaL_checknumber(L, 2));
    float sy = static_cast<float>(luaL_checknumber(L, 3));
    float sz = static_cast<float>(luaL_checknumber(L, 4));
    float angle = static_cast<float>(luaL_checknumber(L, 5));
    float opacity = static_cast<float>(luaL_optnumber(L, 6, 0.0));
    dse_decal_set(entity, sx, sy, sz, angle, opacity);
    return 0;
}

int L_dse_mesh_renderer_add(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* mesh_path = luaL_checkstring(L, 2);
    dse_mesh_renderer_add(entity, mesh_path);
    return 0;
}

int L_dse_mesh_set_depth_state(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int depth_test = static_cast<int>(luaL_checkinteger(L, 2));
    int depth_write = static_cast<int>(luaL_checkinteger(L, 3));
    dse_mesh_set_depth_state(entity, depth_test, depth_write);
    return 0;
}

int L_dse_mesh_set_emissive(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float r = static_cast<float>(luaL_checknumber(L, 2));
    float g = static_cast<float>(luaL_checknumber(L, 3));
    float b = static_cast<float>(luaL_checknumber(L, 4));
    dse_mesh_set_emissive(entity, r, g, b);
    return 0;
}

int L_dse_mesh_set_material(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    dse_mesh_set_material(entity, path);
    return 0;
}

int L_dse_mesh_set_material_scalar(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* param_name = luaL_checkstring(L, 2);
    float value = static_cast<float>(luaL_checknumber(L, 3));
    dse_mesh_set_material_scalar(entity, param_name, value);
    return 0;
}

int L_dse_mesh_set_texture_handle(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* slot_name = luaL_checkstring(L, 2);
    uint32_t handle = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    dse_mesh_set_texture_handle(entity, slot_name, handle);
    return 0;
}

int L_dse_physics3d_boxcast(lua_State* L) {
    uint32_t hit_entity = 0;
    float point[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    float dist = 0;
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
    int _ret = dse_physics3d_boxcast(ox, oy, oz, dx, dy, dz, hx, hy, hz, max_dist, &hit_entity, point, normal, &dist);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(hit_entity));
    lua_pushnumber(L, point[0]);
    lua_pushnumber(L, point[1]);
    lua_pushnumber(L, point[2]);
    lua_pushnumber(L, normal[0]);
    lua_pushnumber(L, normal[1]);
    lua_pushnumber(L, normal[2]);
    lua_pushnumber(L, dist);
    return 9;
}

int L_dse_physics3d_spherecast(lua_State* L) {
    uint32_t hit_entity = 0;
    float point[3] = {0, 0, 0};
    float normal[3] = {0, 0, 0};
    float dist = 0;
    float ox = static_cast<float>(luaL_checknumber(L, 1));
    float oy = static_cast<float>(luaL_checknumber(L, 2));
    float oz = static_cast<float>(luaL_checknumber(L, 3));
    float dx = static_cast<float>(luaL_checknumber(L, 4));
    float dy = static_cast<float>(luaL_checknumber(L, 5));
    float dz = static_cast<float>(luaL_checknumber(L, 6));
    float radius = static_cast<float>(luaL_checknumber(L, 7));
    float max_dist = static_cast<float>(luaL_checknumber(L, 8));
    int _ret = dse_physics3d_spherecast(ox, oy, oz, dx, dy, dz, radius, max_dist, &hit_entity, point, normal, &dist);
    lua_pushinteger(L, _ret);
    lua_pushinteger(L, static_cast<lua_Integer>(hit_entity));
    lua_pushnumber(L, point[0]);
    lua_pushnumber(L, point[1]);
    lua_pushnumber(L, point[2]);
    lua_pushnumber(L, normal[0]);
    lua_pushnumber(L, normal[1]);
    lua_pushnumber(L, normal[2]);
    lua_pushnumber(L, dist);
    return 9;
}

int L_dse_rigidbody3d_set_kinematic(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int kinematic = static_cast<int>(luaL_checkinteger(L, 2));
    dse_rigidbody3d_set_kinematic(entity, kinematic);
    return 0;
}

int L_dse_rigidbody3d_get_linear_damping(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_rigidbody3d_get_linear_damping(entity);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_rigidbody3d_set_linear_damping(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    dse_rigidbody3d_set_linear_damping(entity, value);
    return 0;
}

int L_dse_rigidbody3d_get_angular_damping(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_rigidbody3d_get_angular_damping(entity);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_rigidbody3d_set_angular_damping(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float value = static_cast<float>(luaL_checknumber(L, 2));
    dse_rigidbody3d_set_angular_damping(entity, value);
    return 0;
}

int L_dse_rigidbody3d_add_force_at_position(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fx = static_cast<float>(luaL_checknumber(L, 2));
    float fy = static_cast<float>(luaL_checknumber(L, 3));
    float fz = static_cast<float>(luaL_checknumber(L, 4));
    float px = static_cast<float>(luaL_checknumber(L, 5));
    float py = static_cast<float>(luaL_checknumber(L, 6));
    float pz = static_cast<float>(luaL_checknumber(L, 7));
    dse_rigidbody3d_add_force_at_position(entity, fx, fy, fz, px, py, pz);
    return 0;
}

int L_dse_rendering_set_light_probe(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    dse_rendering_set_light_probe(entity, radius);
    return 0;
}

int L_dse_rendering_set_reflection_probe(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float radius = static_cast<float>(luaL_checknumber(L, 2));
    int resolution = static_cast<int>(luaL_optinteger(L, 3, 128));
    dse_rendering_set_reflection_probe(entity, radius, resolution);
    return 0;
}

int L_dse_post_process_get_state(lua_State* L) {
    int enabled = 0;
    int bloom = 0;
    int ssao = 0;
    int ssr = 0;
    int fxaa = 0;
    int dof = 0;
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_post_process_get_state(entity, &enabled, &bloom, &ssao, &ssr, &fxaa, &dof);
    lua_pushinteger(L, enabled);
    lua_pushinteger(L, bloom);
    lua_pushinteger(L, ssao);
    lua_pushinteger(L, ssr);
    lua_pushinteger(L, fxaa);
    lua_pushinteger(L, dof);
    return 6;
}

} // namespace

void RegisterFreeFn_ecs_gap(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"transform_add", L_dse_transform_add},
        {"anim3d_get_blend_param", L_dse_anim3d_get_blend_param},
        {"anim3d_set_blend_param", L_dse_anim3d_set_blend_param},
        {"anim3d_get_layer_weight", L_dse_anim3d_get_layer_weight},
        {"anim3d_set_layer_weight", L_dse_anim3d_set_layer_weight},
        {"character_check_ground", L_dse_character_check_ground},
        {"decal_add", L_dse_decal_add},
        {"decal_set", L_dse_decal_set},
        {"mesh_renderer_add", L_dse_mesh_renderer_add},
        {"mesh_set_depth_state", L_dse_mesh_set_depth_state},
        {"mesh_set_emissive", L_dse_mesh_set_emissive},
        {"mesh_set_material", L_dse_mesh_set_material},
        {"mesh_set_material_scalar", L_dse_mesh_set_material_scalar},
        {"mesh_set_texture_handle", L_dse_mesh_set_texture_handle},
        {"physics_3d_boxcast", L_dse_physics3d_boxcast},
        {"physics_3d_spherecast", L_dse_physics3d_spherecast},
        {"rigidbody_3d_set_kinematic", L_dse_rigidbody3d_set_kinematic},
        {"rigidbody_3d_get_linear_damping", L_dse_rigidbody3d_get_linear_damping},
        {"rigidbody_3d_set_linear_damping", L_dse_rigidbody3d_set_linear_damping},
        {"rigidbody_3d_get_angular_damping", L_dse_rigidbody3d_get_angular_damping},
        {"rigidbody_3d_set_angular_damping", L_dse_rigidbody3d_set_angular_damping},
        {"rigidbody_3d_add_force_at_position", L_dse_rigidbody3d_add_force_at_position},
        {"set_light_probe", L_dse_rendering_set_light_probe},
        {"set_reflection_probe", L_dse_rendering_set_reflection_probe},
        {"post_process_get_state", L_dse_post_process_get_state},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
