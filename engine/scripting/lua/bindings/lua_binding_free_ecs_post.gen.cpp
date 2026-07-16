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
#include <cmath>
#include <vector>
#include <string>
#include <cstring>
#include <cstdint>

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
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
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

int L_add_post_process(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int enabled = helper::CheckBool(L, 2) ? 1 : 0;
    float bloom_threshold = static_cast<float>(luaL_checknumber(L, 3));
    float bloom_intensity = static_cast<float>(luaL_checknumber(L, 4));
    float bloom_knee = static_cast<float>(luaL_checknumber(L, 5));
    dse_post_process_add(e);
    dse_post_process_set_enabled(e, enabled);
    dse_post_process_set_bloom_enabled(e, 1);
    dse_post_process_set_bloom_threshold(e, bloom_threshold);
    dse_post_process_set_bloom_intensity(e, bloom_intensity);
    dse_post_process_set_bloom_knee(e, bloom_knee);
    return 0;
}

} // namespace

void RegisterEcsRenderingPostBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "ecs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "ecs");
    }
    helper::RegisterBindings(L, {
        {"set_post_process_color_lut", L_dse_post_process_set_color_lut},
        {"add_decal", L_dse_decal_add_simple},
        {"set_decal", L_dse_decal_set_full},
        {"add_post_process", L_add_post_process},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
