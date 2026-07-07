/**
 * @file lua_binding_free_app.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
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
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
