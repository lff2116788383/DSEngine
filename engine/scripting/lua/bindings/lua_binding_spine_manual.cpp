/**
 * @file lua_binding_spine.cpp
 * @brief Lua Spine 绑定 — C ABI 薄包装
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

// dse.spine.add_renderer(entity, skel_path, atlas_path)
int L_SpineAddRenderer(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* skel_path = luaL_checkstring(L, 2);
    const char* atlas_path = luaL_checkstring(L, 3);
    dse_spine_add_renderer(e, skel_path, atlas_path);
    return 0;
}

// dse.spine.set_animation(entity, anim_name, loop)
int L_SpineSetAnimation(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* anim_name = luaL_checkstring(L, 2);
    int loop = lua_toboolean(L, 3) ? 1 : 0;
    dse_spine_set_animation(e, anim_name, loop);
    return 0;
}

void RegisterSpineBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("add_renderer", L_SpineAddRenderer);
    set_fn("set_animation", L_SpineSetAnimation);
}

}
