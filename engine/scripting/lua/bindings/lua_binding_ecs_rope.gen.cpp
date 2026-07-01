/**
 * @file lua_binding_ecs_rope.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * RopeComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_rope_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_rope_get_enabled(e));
    return 1;
}
int L_Set_rope_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_rope_segment_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rope_get_segment_count(e));
    return 1;
}
int L_Set_rope_segment_count(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_segment_count(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_rope_segment_length(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rope_get_segment_length(e));
    return 1;
}
int L_Set_rope_segment_length(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_segment_length(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rope_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rope_get_radius(e));
    return 1;
}
int L_Set_rope_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rope_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rope_get_damping(e));
    return 1;
}
int L_Set_rope_damping(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_damping(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rope_solver_iterations(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rope_get_solver_iterations(e));
    return 1;
}
int L_Set_rope_solver_iterations(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_solver_iterations(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_rope_use_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_rope_get_use_gravity(e));
    return 1;
}
int L_Set_rope_use_gravity(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_use_gravity(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_rope_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_rope_get_gravity_scale(e));
    return 1;
}
int L_Set_rope_gravity_scale(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_gravity_scale(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_rope_anchor_entity_a(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rope_get_anchor_entity_a(e));
    return 1;
}
int L_Set_rope_anchor_entity_a(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_anchor_entity_a(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_rope_anchor_entity_b(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_rope_get_anchor_entity_b(e));
    return 1;
}
int L_Set_rope_anchor_entity_b(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_anchor_entity_b(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_rope_anchor_offset_a(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_rope_get_anchor_offset_a(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_rope_anchor_offset_a(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_anchor_offset_a(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_rope_anchor_offset_b(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_rope_get_anchor_offset_b(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_rope_anchor_offset_b(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_anchor_offset_b(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_rope_start_position(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_rope_get_start_position(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_rope_start_position(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_rope_set_start_position(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

} // namespace

void RegisterRopeComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_rope_enabled", L_Get_rope_enabled},
        {"set_rope_enabled", L_Set_rope_enabled},
        {"get_rope_segment_count", L_Get_rope_segment_count},
        {"set_rope_segment_count", L_Set_rope_segment_count},
        {"get_rope_segment_length", L_Get_rope_segment_length},
        {"set_rope_segment_length", L_Set_rope_segment_length},
        {"get_rope_radius", L_Get_rope_radius},
        {"set_rope_radius", L_Set_rope_radius},
        {"get_rope_damping", L_Get_rope_damping},
        {"set_rope_damping", L_Set_rope_damping},
        {"get_rope_solver_iterations", L_Get_rope_solver_iterations},
        {"set_rope_solver_iterations", L_Set_rope_solver_iterations},
        {"get_rope_use_gravity", L_Get_rope_use_gravity},
        {"set_rope_use_gravity", L_Set_rope_use_gravity},
        {"get_rope_gravity_scale", L_Get_rope_gravity_scale},
        {"set_rope_gravity_scale", L_Set_rope_gravity_scale},
        {"get_rope_anchor_entity_a", L_Get_rope_anchor_entity_a},
        {"set_rope_anchor_entity_a", L_Set_rope_anchor_entity_a},
        {"get_rope_anchor_entity_b", L_Get_rope_anchor_entity_b},
        {"set_rope_anchor_entity_b", L_Set_rope_anchor_entity_b},
        {"get_rope_anchor_offset_a", L_Get_rope_anchor_offset_a},
        {"set_rope_anchor_offset_a", L_Set_rope_anchor_offset_a},
        {"get_rope_anchor_offset_b", L_Get_rope_anchor_offset_b},
        {"set_rope_anchor_offset_b", L_Set_rope_anchor_offset_b},
        {"get_rope_start_position", L_Get_rope_start_position},
        {"set_rope_start_position", L_Set_rope_start_position},
    });
}

} // namespace dse::runtime::lua_binding
