/**
 * @file lua_binding_free_ecs_post.gen.cpp
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

int L_dse_post_process_set_color_lut(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    float intensity = static_cast<float>(luaL_checknumber(L, 3));
    dse_post_process_set_color_lut(e, path, intensity);
    return 0;
}

int L_dse_decal_add_simple(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_decal_add_simple(e);
    return 0;
}

int L_dse_decal_set_full(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = static_cast<int>(luaL_checkinteger(L, 2));
    int has_texture = static_cast<int>(luaL_checkinteger(L, 3));
    uint32_t texture = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    float r = static_cast<float>(luaL_checknumber(L, 5));
    float g = static_cast<float>(luaL_checknumber(L, 6));
    float b = static_cast<float>(luaL_checknumber(L, 7));
    float a = static_cast<float>(luaL_checknumber(L, 8));
    float angle_fade = static_cast<float>(luaL_checknumber(L, 9));
    dse_decal_set_full(e, enabled, has_texture, texture, r, g, b, a, angle_fade);
    return 0;
}

} // namespace

void RegisterEcsRenderingPostBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    helper::RegisterBindings(L, {
        {"set_post_process_color_lut", L_dse_post_process_set_color_lut},
        {"add_decal", L_dse_decal_add_simple},
        {"set_decal", L_dse_decal_set_full},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
