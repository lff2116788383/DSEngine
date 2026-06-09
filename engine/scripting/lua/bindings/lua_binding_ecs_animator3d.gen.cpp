/**
 * @file lua_binding_ecs_animator3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * Animator3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_animator3d_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_animator3d_get_enabled(e));
    return 1;
}
int L_Set_animator3d_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_animator3d_danim_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_animator3d_get_danim_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_animator3d_danim_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_danim_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_animator3d_dskel_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_animator3d_get_dskel_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_animator3d_dskel_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_dskel_path(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_animator3d_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_animator3d_get_speed(e));
    return 1;
}
int L_Set_animator3d_speed(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_speed(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_animator3d_loop(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_animator3d_get_loop(e));
    return 1;
}
int L_Set_animator3d_loop(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_loop(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_animator3d_use_anim_tree(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_animator3d_get_use_anim_tree(e));
    return 1;
}
int L_Set_animator3d_use_anim_tree(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_use_anim_tree(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_animator3d_blend_parameter(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[128];
    if (dse_animator3d_get_blend_parameter(e, buf, 128) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_animator3d_blend_parameter(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_blend_parameter(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_animator3d_blend_parameter_value(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_animator3d_get_blend_parameter_value(e));
    return 1;
}
int L_Set_animator3d_blend_parameter_value(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_animator3d_set_blend_parameter_value(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterAnimator3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_animator3d_enabled", L_Get_animator3d_enabled},
        {"set_animator3d_enabled", L_Set_animator3d_enabled},
        {"get_animator3d_danim_path", L_Get_animator3d_danim_path},
        {"set_animator3d_danim_path", L_Set_animator3d_danim_path},
        {"get_animator3d_dskel_path", L_Get_animator3d_dskel_path},
        {"set_animator3d_dskel_path", L_Set_animator3d_dskel_path},
        {"get_animator3d_speed", L_Get_animator3d_speed},
        {"set_animator3d_speed", L_Set_animator3d_speed},
        {"get_animator3d_loop", L_Get_animator3d_loop},
        {"set_animator3d_loop", L_Set_animator3d_loop},
        {"get_animator3d_use_anim_tree", L_Get_animator3d_use_anim_tree},
        {"set_animator3d_use_anim_tree", L_Set_animator3d_use_anim_tree},
        {"get_animator3d_blend_parameter", L_Get_animator3d_blend_parameter},
        {"set_animator3d_blend_parameter", L_Set_animator3d_blend_parameter},
        {"get_animator3d_blend_parameter_value", L_Get_animator3d_blend_parameter_value},
        {"set_animator3d_blend_parameter_value", L_Set_animator3d_blend_parameter_value},
    });
}

} // namespace dse::runtime::lua_binding
