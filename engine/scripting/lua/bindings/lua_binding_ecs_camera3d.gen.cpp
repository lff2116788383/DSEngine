/**
 * @file lua_binding_ecs_camera3d.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * Camera3DComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_camera3d_fov(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_camera3d_get_fov(e));
    return 1;
}
int L_Set_camera3d_fov(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_camera3d_set_fov(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_camera3d_near_clip(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_camera3d_get_near_clip(e));
    return 1;
}
int L_Set_camera3d_near_clip(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_camera3d_set_near_clip(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_camera3d_far_clip(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_camera3d_get_far_clip(e));
    return 1;
}
int L_Set_camera3d_far_clip(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_camera3d_set_far_clip(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_camera3d_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_camera3d_get_enabled(e));
    return 1;
}
int L_Set_camera3d_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_camera3d_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_camera3d_priority(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_camera3d_get_priority(e));
    return 1;
}
int L_Set_camera3d_priority(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_camera3d_set_priority(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

} // namespace

void RegisterCamera3DComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_camera3d_fov", L_Get_camera3d_fov},
        {"set_camera3d_fov", L_Set_camera3d_fov},
        {"get_camera3d_near_clip", L_Get_camera3d_near_clip},
        {"set_camera3d_near_clip", L_Set_camera3d_near_clip},
        {"get_camera3d_far_clip", L_Get_camera3d_far_clip},
        {"set_camera3d_far_clip", L_Set_camera3d_far_clip},
        {"get_camera3d_enabled", L_Get_camera3d_enabled},
        {"set_camera3d_enabled", L_Set_camera3d_enabled},
        {"get_camera3d_priority", L_Get_camera3d_priority},
        {"set_camera3d_priority", L_Set_camera3d_priority},
    });
}

} // namespace dse::runtime::lua_binding
