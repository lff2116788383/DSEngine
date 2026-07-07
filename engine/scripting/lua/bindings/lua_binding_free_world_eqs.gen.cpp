/**
 * @file lua_binding_free_world_eqs.gen.cpp
 * @brief auto-generated -- do not edit
 *        source: tools/codegen/function_defs.json
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}
#include <vector>
#include <string>
#include <cstring>

namespace dse::runtime::lua_binding {
namespace {

int L_dse_eqs_init(lua_State* L) {
    int _ret = dse_eqs_init();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_eqs_shutdown(lua_State* L) {
    dse_eqs_shutdown();
    return 0;
}

int L_dse_eqs_create_template(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    uint32_t _ret = dse_eqs_create_template(name);
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

int L_dse_eqs_destroy_template(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_eqs_destroy_template(id);
    return 0;
}

int L_dse_eqs_set_generator(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int gen_type = static_cast<int>(luaL_checkinteger(L, 2));
    float radius = static_cast<float>(luaL_optnumber(L, 3, 20.0));
    float spacing = static_cast<float>(luaL_optnumber(L, 4, 2.0));
    int max_items = static_cast<int>(luaL_optinteger(L, 5, 200));
    dse_eqs_set_generator(id, gen_type, radius, spacing, max_items);
    return 0;
}

int L_dse_eqs_add_scorer(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int scorer_type = static_cast<int>(luaL_checkinteger(L, 2));
    float weight = static_cast<float>(luaL_optnumber(L, 3, 1.0));
    int invert = helper::OptBool(L, 4, false) ? 1 : 0;
    float max_dist = static_cast<float>(luaL_optnumber(L, 5, 100.0));
    dse_eqs_add_scorer(id, scorer_type, weight, invert, max_dist);
    return 0;
}

int L_dse_eqs_clear_scorers(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_eqs_clear_scorers(id);
    return 0;
}

int L_dse_eqs_set_combine_mode(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    int mode = static_cast<int>(luaL_checkinteger(L, 2));
    dse_eqs_set_combine_mode(id, mode);
    return 0;
}

int L_dse_eqs_set_max_results(lua_State* L) {
    uint32_t id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    uint32_t max_results = static_cast<uint32_t>(luaL_checkinteger(L, 2));
    dse_eqs_set_max_results(id, max_results);
    return 0;
}

int L_dse_eqs_execute(lua_State* L) {
    float out[7] = { 0, 0, 0, 0, 0, 0, 0 };
    uint32_t template_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    dse_eqs_execute(template_id, cx, cy, cz, out);
    lua_newtable(L);
    lua_pushnumber(L, out[0]);
    lua_setfield(L, -2, "best_x");
    lua_pushnumber(L, out[1]);
    lua_setfield(L, -2, "best_y");
    lua_pushnumber(L, out[2]);
    lua_setfield(L, -2, "best_z");
    lua_pushnumber(L, out[3]);
    lua_setfield(L, -2, "best_score");
    lua_pushinteger(L, static_cast<lua_Integer>(out[4]));
    lua_setfield(L, -2, "total_generated");
    lua_pushinteger(L, static_cast<lua_Integer>(out[5]));
    lua_setfield(L, -2, "valid_count");
    lua_pushnumber(L, out[6]);
    lua_setfield(L, -2, "query_time_ms");
    return 1;
}

int L_dse_eqs_get_template_count(lua_State* L) {
    int _ret = dse_eqs_get_template_count();
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_eqs_execute_at(lua_State* L) {
    float out[5] = { 0, 0, 0, 0, 0 };
    uint32_t template_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    float tx = static_cast<float>(luaL_checknumber(L, 5));
    float ty = static_cast<float>(luaL_checknumber(L, 6));
    float tz = static_cast<float>(luaL_checknumber(L, 7));
    dse_eqs_execute_at(template_id, cx, cy, cz, tx, ty, tz, out);
    lua_newtable(L);
    lua_pushnumber(L, out[0]);
    lua_setfield(L, -2, "best_x");
    lua_pushnumber(L, out[1]);
    lua_setfield(L, -2, "best_y");
    lua_pushnumber(L, out[2]);
    lua_setfield(L, -2, "best_z");
    lua_pushnumber(L, out[3]);
    lua_setfield(L, -2, "best_score");
    lua_pushinteger(L, static_cast<lua_Integer>(out[4]));
    lua_setfield(L, -2, "valid_count");
    return 1;
}

} // namespace

void RegisterFreeWorldEqsBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "eqs");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "eqs");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_eqs_init},
        {"shutdown", L_dse_eqs_shutdown},
        {"create_template", L_dse_eqs_create_template},
        {"destroy_template", L_dse_eqs_destroy_template},
        {"set_generator", L_dse_eqs_set_generator},
        {"add_scorer", L_dse_eqs_add_scorer},
        {"clear_scorers", L_dse_eqs_clear_scorers},
        {"set_combine_mode", L_dse_eqs_set_combine_mode},
        {"set_max_results", L_dse_eqs_set_max_results},
        {"execute", L_dse_eqs_execute},
        {"get_template_count", L_dse_eqs_get_template_count},
        {"execute_at", L_dse_eqs_execute_at},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
