/**
 * @file lua_binding_free_world_vsm.gen.cpp
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

int L_dse_vsm_init(lua_State* L) {
    uint32_t virtual_res = 16384;
    uint32_t page_size = 128;
    uint32_t pool_pages = 512;
    uint32_t clipmap_levels = 6;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "virtual_resolution");
        if (!lua_isnil(L, -1)) virtual_res = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "page_size");
        if (!lua_isnil(L, -1)) page_size = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "pool_pages");
        if (!lua_isnil(L, -1)) pool_pages = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "clipmap_levels");
        if (!lua_isnil(L, -1)) clipmap_levels = static_cast<uint32_t>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }
    int _ret = dse_vsm_init(virtual_res, page_size, pool_pages, clipmap_levels);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_vsm_shutdown(lua_State* L) {
    dse_vsm_shutdown();
    return 0;
}

int L_dse_vsm_register_light(lua_State* L) {
    uint32_t entity = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int is_directional = helper::CheckBool(L, 2) ? 1 : 0;
    float dx = static_cast<float>(luaL_optnumber(L, 3, 0));
    float dy = static_cast<float>(luaL_optnumber(L, 4, -1));
    float dz = static_cast<float>(luaL_optnumber(L, 5, 0));
    uint32_t _ret = dse_vsm_register_light(entity, is_directional, dx, dy, dz);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_vsm_unregister_light(lua_State* L) {
    uint32_t light = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_vsm_unregister_light(light);
    return 0;
}

int L_dse_vsm_begin_frame(lua_State* L) {
    uint32_t light = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    dse_vsm_begin_frame(light, cx, cy, cz);
    return 0;
}

int L_dse_vsm_end_frame(lua_State* L) {
    dse_vsm_end_frame();
    return 0;
}

int L_dse_vsm_invalidate(lua_State* L) {
    uint32_t light = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float min_x = static_cast<float>(luaL_checknumber(L, 2));
    float min_y = static_cast<float>(luaL_checknumber(L, 3));
    float min_z = static_cast<float>(luaL_checknumber(L, 4));
    float max_x = static_cast<float>(luaL_checknumber(L, 5));
    float max_y = static_cast<float>(luaL_checknumber(L, 6));
    float max_z = static_cast<float>(luaL_checknumber(L, 7));
    dse_vsm_invalidate(light, min_x, min_y, min_z, max_x, max_y, max_z);
    return 0;
}

int L_dse_vsm_mark_page_rendered(lua_State* L) {
    uint32_t light = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t px = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t py = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    uint32_t level = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    dse_vsm_mark_page_rendered(light, px, py, level);
    return 0;
}

int L_dse_vsm_get_pages_to_render(lua_State* L) {
    int _ret = dse_vsm_get_pages_to_render();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_vsm_lookup_page(lua_State* L) {
    uint32_t px = 0;
    uint32_t py = 0;
    uint32_t light = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t vx = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    uint32_t vy = static_cast<uint32_t>(luaL_checkinteger(L, 3));
    uint32_t level = static_cast<uint32_t>(luaL_checkinteger(L, 4));
    int _ret = dse_vsm_lookup_page(light, vx, vy, level, &px, &py);
    if (_ret) {
        lua_pushboolean(L, 1);
        lua_pushinteger(L, static_cast<lua_Integer>(px));
        lua_pushinteger(L, static_cast<lua_Integer>(py));
        return 3;
    }
    lua_pushboolean(L, 0);
    return 1;
}

int L_dse_vsm_get_stats(lua_State* L) {
    int total = 0;
    int mapped = 0;
    int dirty = 0;
    int rendered = 0;
    int cache_hit = 0;
    int pool_usage = 0;
    dse_vsm_get_stats(&total, &mapped, &dirty, &rendered, &cache_hit, &pool_usage);
    lua_newtable(L);
    lua_pushinteger(L, total);
    lua_setfield(L, -2, "total_pages");
    lua_pushinteger(L, mapped);
    lua_setfield(L, -2, "mapped_pages");
    lua_pushinteger(L, dirty);
    lua_setfield(L, -2, "dirty_pages");
    lua_pushinteger(L, rendered);
    lua_setfield(L, -2, "rendered_this_frame");
    lua_pushinteger(L, cache_hit);
    lua_setfield(L, -2, "cache_hit_percent");
    lua_pushinteger(L, pool_usage);
    lua_setfield(L, -2, "pool_usage_percent");
    return 1;
}

int L_dse_vsm_get_clipmap_levels(lua_State* L) {
    int _ret = dse_vsm_get_clipmap_levels();
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeWorldVsmBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "vsm");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "vsm");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_vsm_init},
        {"shutdown", L_dse_vsm_shutdown},
        {"register_light", L_dse_vsm_register_light},
        {"unregister_light", L_dse_vsm_unregister_light},
        {"begin_frame", L_dse_vsm_begin_frame},
        {"end_frame", L_dse_vsm_end_frame},
        {"invalidate", L_dse_vsm_invalidate},
        {"mark_rendered", L_dse_vsm_mark_page_rendered},
        {"get_pages_to_render", L_dse_vsm_get_pages_to_render},
        {"lookup_page", L_dse_vsm_lookup_page},
        {"get_stats", L_dse_vsm_get_stats},
        {"get_clipmap_levels", L_dse_vsm_get_clipmap_levels},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
