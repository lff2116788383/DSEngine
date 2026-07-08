/**
 * @file lua_binding_free_world_spline.gen.cpp
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

int L_dse_spline_init(lua_State* L) {
    int _ret = dse_spline_init();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_spline_shutdown(lua_State* L) {
    dse_spline_shutdown();
    return 0;
}

int L_dse_spline_create(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    uint32_t _ret = dse_spline_create(name);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_spline_destroy(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_spline_destroy(id);
    return 0;
}

int L_dse_spline_add_point(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float width = static_cast<float>(luaL_optnumber(L, 5, 4.0));
    dse_spline_add_point(id, x, y, z, width);
    return 0;
}

int L_dse_spline_set_point(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int index = static_cast<int>(luaL_checkinteger(L, 2));
    float x = static_cast<float>(luaL_checknumber(L, 3));
    float y = static_cast<float>(luaL_checknumber(L, 4));
    float z = static_cast<float>(luaL_checknumber(L, 5));
    float width = static_cast<float>(luaL_optnumber(L, 6, 4.0));
    dse_spline_set_point(id, index, x, y, z, width);
    return 0;
}

int L_dse_spline_remove_point(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int index = static_cast<int>(luaL_checkinteger(L, 2));
    dse_spline_remove_point(id, index);
    return 0;
}

int L_dse_spline_get_point_count(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int _ret = dse_spline_get_point_count(id);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_spline_get_length(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float _ret = dse_spline_get_length(id);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_spline_evaluate(lua_State* L) {
    float xyz[3] = {0, 0, 0};
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float t = static_cast<float>(luaL_checknumber(L, 2));
    dse_spline_evaluate(id, t, xyz);
    lua_pushnumber(L, xyz[0]);
    lua_pushnumber(L, xyz[1]);
    lua_pushnumber(L, xyz[2]);
    return 3;
}

int L_dse_spline_evaluate_distance(lua_State* L) {
    float xyz[3] = {0, 0, 0};
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float dist = static_cast<float>(luaL_checknumber(L, 2));
    dse_spline_evaluate_distance(id, dist, xyz);
    lua_pushnumber(L, xyz[0]);
    lua_pushnumber(L, xyz[1]);
    lua_pushnumber(L, xyz[2]);
    return 3;
}

int L_dse_spline_find_nearest(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float x = static_cast<float>(luaL_checknumber(L, 2));
    float y = static_cast<float>(luaL_checknumber(L, 3));
    float z = static_cast<float>(luaL_checknumber(L, 4));
    float _ret = dse_spline_find_nearest(id, x, y, z);
    lua_pushnumber(L, _ret);
    return 1;
}

int L_dse_spline_gen_road(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float step = static_cast<float>(luaL_optnumber(L, 2, 1.0));
    int segments = static_cast<int>(luaL_optinteger(L, 3, 4));
    int _ret = dse_spline_gen_road(id, step, segments);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_spline_gen_river(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float width = static_cast<float>(luaL_optnumber(L, 2, 2.0));
    float depth = static_cast<float>(luaL_optnumber(L, 3, 2.0));
    int _ret = dse_spline_gen_river(id, width, depth);
    lua_pushinteger(L, _ret);
    return 1;
}

} // namespace

void RegisterFreeWorldSplineBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "spline");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "spline");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_spline_init},
        {"shutdown", L_dse_spline_shutdown},
        {"create", L_dse_spline_create},
        {"destroy", L_dse_spline_destroy},
        {"add_point", L_dse_spline_add_point},
        {"set_point", L_dse_spline_set_point},
        {"remove_point", L_dse_spline_remove_point},
        {"get_point_count", L_dse_spline_get_point_count},
        {"get_length", L_dse_spline_get_length},
        {"evaluate", L_dse_spline_evaluate},
        {"evaluate_distance", L_dse_spline_evaluate_distance},
        {"find_nearest", L_dse_spline_find_nearest},
        {"gen_road", L_dse_spline_gen_road},
        {"gen_river", L_dse_spline_gen_river},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
