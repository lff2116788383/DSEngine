/**
 * @file lua_binding_ecs_impostor.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * ImpostorComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_impostor_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_impostor_get_enabled(e));
    return 1;
}
int L_Set_impostor_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_impostor_atlas_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_impostor_get_atlas_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_impostor_atlas_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_atlas_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_impostor_frames_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_impostor_get_frames_x(e));
    return 1;
}
int L_Set_impostor_frames_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_frames_x(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_impostor_frames_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_impostor_get_frames_y(e));
    return 1;
}
int L_Set_impostor_frames_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_frames_y(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_impostor_transition_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_impostor_get_transition_distance(e));
    return 1;
}
int L_Set_impostor_transition_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_transition_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_impostor_fade_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_impostor_get_fade_range(e));
    return 1;
}
int L_Set_impostor_fade_range(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_fade_range(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_impostor_cull_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_impostor_get_cull_distance(e));
    return 1;
}
int L_Set_impostor_cull_distance(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_cull_distance(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_impostor_impostor_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_impostor_get_impostor_size(e));
    return 1;
}
int L_Set_impostor_impostor_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_impostor_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_impostor_pivot_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_impostor_get_pivot_offset(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_impostor_pivot_offset(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_pivot_offset(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_impostor_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_impostor_get_cast_shadow(e));
    return 1;
}
int L_Set_impostor_cast_shadow(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_cast_shadow(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_impostor_use_frame_interpolation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_impostor_get_use_frame_interpolation(e));
    return 1;
}
int L_Set_impostor_use_frame_interpolation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_use_frame_interpolation(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_impostor_normal_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_impostor_get_normal_strength(e));
    return 1;
}
int L_Set_impostor_normal_strength(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_normal_strength(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_impostor_auto_from_lod_group(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_impostor_get_auto_from_lod_group(e));
    return 1;
}
int L_Set_impostor_auto_from_lod_group(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_impostor_set_auto_from_lod_group(e, lua_toboolean(L, 2));
    return 0;
}

} // namespace

void RegisterImpostorComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_impostor_enabled", L_Get_impostor_enabled},
        {"set_impostor_enabled", L_Set_impostor_enabled},
        {"get_impostor_atlas_path", L_Get_impostor_atlas_path},
        {"set_impostor_atlas_path", L_Set_impostor_atlas_path},
        {"get_impostor_frames_x", L_Get_impostor_frames_x},
        {"set_impostor_frames_x", L_Set_impostor_frames_x},
        {"get_impostor_frames_y", L_Get_impostor_frames_y},
        {"set_impostor_frames_y", L_Set_impostor_frames_y},
        {"get_impostor_transition_distance", L_Get_impostor_transition_distance},
        {"set_impostor_transition_distance", L_Set_impostor_transition_distance},
        {"get_impostor_fade_range", L_Get_impostor_fade_range},
        {"set_impostor_fade_range", L_Set_impostor_fade_range},
        {"get_impostor_cull_distance", L_Get_impostor_cull_distance},
        {"set_impostor_cull_distance", L_Set_impostor_cull_distance},
        {"get_impostor_impostor_size", L_Get_impostor_impostor_size},
        {"set_impostor_impostor_size", L_Set_impostor_impostor_size},
        {"get_impostor_pivot_offset", L_Get_impostor_pivot_offset},
        {"set_impostor_pivot_offset", L_Set_impostor_pivot_offset},
        {"get_impostor_cast_shadow", L_Get_impostor_cast_shadow},
        {"set_impostor_cast_shadow", L_Set_impostor_cast_shadow},
        {"get_impostor_use_frame_interpolation", L_Get_impostor_use_frame_interpolation},
        {"set_impostor_use_frame_interpolation", L_Set_impostor_use_frame_interpolation},
        {"get_impostor_normal_strength", L_Get_impostor_normal_strength},
        {"set_impostor_normal_strength", L_Set_impostor_normal_strength},
        {"get_impostor_auto_from_lod_group", L_Get_impostor_auto_from_lod_group},
        {"set_impostor_auto_from_lod_group", L_Set_impostor_auto_from_lod_group},
    });
}

} // namespace dse::runtime::lua_binding
