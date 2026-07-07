/**
 * @file lua_binding_free_spine.gen.cpp
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

int L_dse_spine_add_renderer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* skel_path = luaL_checkstring(L, 2);
    const char* atlas_path = luaL_checkstring(L, 3);
    dse_spine_add_renderer(e, skel_path, atlas_path);
    return 0;
}

int L_dse_spine_set_animation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* anim_name = luaL_checkstring(L, 2);
    int loop = static_cast<int>(luaL_checkinteger(L, 3));
    dse_spine_set_animation(e, anim_name, loop);
    return 0;
}

} // namespace

void RegisterSpineBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"spineaddrenderer", L_dse_spine_add_renderer},
        {"spinesetanimation", L_dse_spine_set_animation},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
