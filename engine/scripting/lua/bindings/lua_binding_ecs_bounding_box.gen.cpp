/**
 * @file lua_binding_ecs_bounding_box.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * BoundingBoxComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_bounding_box_min_extents(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_bounding_box_get_min_extents(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_bounding_box_min_extents(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_bounding_box_set_min_extents(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}
int L_Get_bounding_box_max_extents(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = 0, y = 0, z = 0;
    dse_bounding_box_get_max_extents(e, &x, &y, &z);
    lua_pushnumber(L, x); lua_pushnumber(L, y); lua_pushnumber(L, z);
    return 3;
}
int L_Set_bounding_box_max_extents(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_bounding_box_set_max_extents(e,
        static_cast<float>(luaL_checknumber(L, 2)),
        static_cast<float>(luaL_checknumber(L, 3)),
        static_cast<float>(luaL_checknumber(L, 4)));
    return 0;
}

} // namespace

void RegisterBoundingBoxComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_bounding_box_min_extents", L_Get_bounding_box_min_extents},
        {"set_bounding_box_min_extents", L_Set_bounding_box_min_extents},
        {"get_bounding_box_max_extents", L_Get_bounding_box_max_extents},
        {"set_bounding_box_max_extents", L_Set_bounding_box_max_extents},
    });
}

} // namespace dse::runtime::lua_binding
