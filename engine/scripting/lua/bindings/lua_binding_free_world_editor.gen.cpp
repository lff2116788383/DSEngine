/**
 * @file lua_binding_free_world_editor.gen.cpp
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

int L_dse_editor_init(lua_State* L) {
    int _ret = dse_editor_init();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_shutdown(lua_State* L) {
    dse_editor_shutdown();
    return 0;
}

int L_dse_editor_terrain_brush(lua_State* L) {
    int mode = static_cast<int>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_checknumber(L, 4));
    float strength = static_cast<float>(luaL_checknumber(L, 5));
    float falloff = static_cast<float>(luaL_optnumber(L, 6, 0.5));
    float opacity = static_cast<float>(luaL_optnumber(L, 7, 0.5));
    int _ret = dse_editor_terrain_brush(mode, cx, cy, radius, strength, falloff, opacity);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_brush_preview(lua_State* L) {
    float min_x = 0;
    float min_y = 0;
    float max_x = 0;
    float max_y = 0;
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float radius = static_cast<float>(luaL_checknumber(L, 3));
    float strength = static_cast<float>(luaL_checknumber(L, 4));
    dse_editor_brush_preview(cx, cy, radius, strength, &min_x, &min_y, &max_x, &max_y);
    lua_pushnumber(L, min_x);
    lua_pushnumber(L, min_y);
    lua_pushnumber(L, max_x);
    lua_pushnumber(L, max_y);
    return 4;
}

int L_dse_editor_place_foliage(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float radius = static_cast<float>(luaL_checknumber(L, 3));
    float density = static_cast<float>(luaL_checknumber(L, 4));
    float scale = static_cast<float>(luaL_optnumber(L, 5, 0.5));
    const char* mesh_name = luaL_optstring(L, 6, "default_tree");
    int _ret = dse_editor_place_foliage(x, y, radius, density, scale, mesh_name);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_erase_foliage(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float radius = static_cast<float>(luaL_checknumber(L, 3));
    float strength = static_cast<float>(luaL_checknumber(L, 4));
    int _ret = dse_editor_erase_foliage(x, y, radius, strength);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_get_foliage_count(lua_State* L) {
    int _ret = dse_editor_get_foliage_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_begin_road(lua_State* L) {
    float width = static_cast<float>(luaL_optnumber(L, 1, 4.0));
    int _ret = dse_editor_begin_road(width);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_add_road_point(lua_State* L) {
    uint32_t road_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    dse_editor_add_road_point(road_id, x, y, z);
    return 0;
}

int L_dse_editor_end_road(lua_State* L) {
    uint32_t road_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_editor_end_road(road_id);
    return 0;
}

int L_dse_editor_update_partition_vis(lua_State* L) {
    float cx = static_cast<float>(luaL_checknumber(L, 1));
    float cy = static_cast<float>(luaL_checknumber(L, 2));
    float cz = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 256.0));
    dse_editor_update_partition_vis(cx, cy, cz, radius);
    return 0;
}

int L_dse_editor_get_cell_count(lua_State* L) {
    int _ret = dse_editor_get_cell_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_editor_undo(lua_State* L) {
    int _ret = dse_editor_undo();
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_editor_redo(lua_State* L) {
    int _ret = dse_editor_redo();
    lua_pushboolean(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeWorldEditorBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "editor");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "editor");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_editor_init},
        {"shutdown", L_dse_editor_shutdown},
        {"terrain_brush", L_dse_editor_terrain_brush},
        {"brush_preview", L_dse_editor_brush_preview},
        {"place_foliage", L_dse_editor_place_foliage},
        {"erase_foliage", L_dse_editor_erase_foliage},
        {"get_foliage_count", L_dse_editor_get_foliage_count},
        {"begin_road", L_dse_editor_begin_road},
        {"add_road_point", L_dse_editor_add_road_point},
        {"end_road", L_dse_editor_end_road},
        {"update_partition_vis", L_dse_editor_update_partition_vis},
        {"get_cell_count", L_dse_editor_get_cell_count},
        {"undo", L_dse_editor_undo},
        {"redo", L_dse_editor_redo},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
