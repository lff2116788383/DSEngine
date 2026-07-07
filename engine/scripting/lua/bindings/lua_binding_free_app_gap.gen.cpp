/**
 * @file lua_binding_free_app_gap.gen.cpp
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

} // namespace

void RegisterFreeAppGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"app_get_fps", L_dse_app_get_fps},
        {"app_get_frame_time_ms", L_dse_app_get_frame_time_ms},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
