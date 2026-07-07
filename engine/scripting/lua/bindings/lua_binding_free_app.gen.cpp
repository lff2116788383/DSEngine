/**
 * @file lua_binding_free_app.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/function_defs.json
 *
 * app 组自由函数的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_dse_app_get_delta_time(lua_State* L) {
    float _ret = dse_app_get_delta_time();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_app_get_target_fps(lua_State* L) {
    float _ret = dse_app_get_target_fps();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_app_get_time(lua_State* L) {
    float _ret = dse_app_get_time();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_input_get_mouse_scroll(lua_State* L) {
    float _ret = dse_input_get_mouse_scroll();
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_input_get_touch_count(lua_State* L) {
    int _ret = dse_input_get_touch_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_input_get_touch(lua_State* L) {
    float x = 0;
    float y = 0;
    int phase = 0;
    int index = static_cast<int>(luaL_checkinteger(L, 1));
    dse_input_get_touch(index, &x, &y, &phase);
    lua_pushnumber(L, x);
    lua_pushnumber(L, y);
    lua_pushinteger(L, phase);
    return 3;
}

} // namespace

void RegisterFreeFn_app(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "app");
    helper::RegisterBindings(L, {
        {"get_delta_time", L_dse_app_get_delta_time},
        {"get_target_fps", L_dse_app_get_target_fps},
        {"get_time", L_dse_app_get_time},
        {"get_mouse_scroll", L_dse_input_get_mouse_scroll},
        {"get_touch_count", L_dse_input_get_touch_count},
        {"get_touch", L_dse_input_get_touch},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
