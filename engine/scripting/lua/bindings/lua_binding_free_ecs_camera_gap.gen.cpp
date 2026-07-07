/**
 * @file lua_binding_free_ecs_camera_gap.gen.cpp
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

int L_dse_camera3d_add(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float fov = static_cast<float>(luaL_checknumber(L, 2));
    float near_clip = static_cast<float>(luaL_checknumber(L, 3));
    float far_clip = static_cast<float>(luaL_checknumber(L, 4));
    dse_camera3d_add(e, fov, near_clip, far_clip);
    return 0;
}

} // namespace

void RegisterFreeCameraGapBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    helper::RegisterBindings(L, {
        {"camera3d_add", L_dse_camera3d_add},
    });
    lua_pop(L, 1);
}

} // namespace dse::runtime::lua_binding
