/**
 * @file lua_binding_ecs_sub_scene.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * SubSceneComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_sub_scene_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_sub_scene_get_enabled(e));
    return 1;
}
int L_Set_sub_scene_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_sub_scene_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_sub_scene_scene_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_sub_scene_get_scene_path(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_sub_scene_scene_path(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_sub_scene_set_scene_path(e, luaL_checkstring(L, 2));
    return 0;
}

} // namespace

void RegisterSubSceneComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_sub_scene_enabled", L_Get_sub_scene_enabled},
        {"set_sub_scene_enabled", L_Set_sub_scene_enabled},
        {"get_sub_scene_scene_path", L_Get_sub_scene_scene_path},
        {"set_sub_scene_scene_path", L_Set_sub_scene_scene_path},
    });
}

} // namespace dse::runtime::lua_binding
