/**
 * @file lua_binding_ecs_navmesh_rebake.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * NavMeshAutoRebakeComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_navmesh_rebake_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_navmesh_rebake_get_enabled(e));
    return 1;
}
int L_Set_navmesh_rebake_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_navmesh_rebake_tile_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_tile_size(e));
    return 1;
}
int L_Set_navmesh_rebake_tile_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_tile_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_rebake_cooldown(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_rebake_cooldown(e));
    return 1;
}
int L_Set_navmesh_rebake_rebake_cooldown(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_rebake_cooldown(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_collect_terrain(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_navmesh_rebake_get_collect_terrain(e));
    return 1;
}
int L_Set_navmesh_rebake_collect_terrain(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_collect_terrain(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_navmesh_rebake_collect_mesh_renderers(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_navmesh_rebake_get_collect_mesh_renderers(e));
    return 1;
}
int L_Set_navmesh_rebake_collect_mesh_renderers(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_collect_mesh_renderers(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_navmesh_rebake_agent_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_agent_height(e));
    return 1;
}
int L_Set_navmesh_rebake_agent_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_agent_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_agent_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_agent_radius(e));
    return 1;
}
int L_Set_navmesh_rebake_agent_radius(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_agent_radius(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_agent_max_climb(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_agent_max_climb(e));
    return 1;
}
int L_Set_navmesh_rebake_agent_max_climb(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_agent_max_climb(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_agent_max_slope(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_agent_max_slope(e));
    return 1;
}
int L_Set_navmesh_rebake_agent_max_slope(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_agent_max_slope(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_cell_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_cell_size(e));
    return 1;
}
int L_Set_navmesh_rebake_cell_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_cell_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_navmesh_rebake_cell_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_navmesh_rebake_get_cell_height(e));
    return 1;
}
int L_Set_navmesh_rebake_cell_height(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_navmesh_rebake_set_cell_height(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}

} // namespace

void RegisterNavMeshAutoRebakeComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_navmesh_auto_rebake_enabled", L_Get_navmesh_rebake_enabled},
        {"set_navmesh_auto_rebake_enabled", L_Set_navmesh_rebake_enabled},
        {"get_navmesh_auto_rebake_tile_size", L_Get_navmesh_rebake_tile_size},
        {"set_navmesh_auto_rebake_tile_size", L_Set_navmesh_rebake_tile_size},
        {"get_navmesh_auto_rebake_cooldown", L_Get_navmesh_rebake_rebake_cooldown},
        {"set_navmesh_auto_rebake_cooldown", L_Set_navmesh_rebake_rebake_cooldown},
        {"get_navmesh_auto_rebake_collect_terrain", L_Get_navmesh_rebake_collect_terrain},
        {"set_navmesh_auto_rebake_collect_terrain", L_Set_navmesh_rebake_collect_terrain},
        {"get_navmesh_auto_rebake_collect_mesh_renderers", L_Get_navmesh_rebake_collect_mesh_renderers},
        {"set_navmesh_auto_rebake_collect_mesh_renderers", L_Set_navmesh_rebake_collect_mesh_renderers},
        {"get_navmesh_auto_rebake_agent_height", L_Get_navmesh_rebake_agent_height},
        {"set_navmesh_auto_rebake_agent_height", L_Set_navmesh_rebake_agent_height},
        {"get_navmesh_auto_rebake_agent_radius", L_Get_navmesh_rebake_agent_radius},
        {"set_navmesh_auto_rebake_agent_radius", L_Set_navmesh_rebake_agent_radius},
        {"get_navmesh_auto_rebake_agent_max_climb", L_Get_navmesh_rebake_agent_max_climb},
        {"set_navmesh_auto_rebake_agent_max_climb", L_Set_navmesh_rebake_agent_max_climb},
        {"get_navmesh_auto_rebake_agent_max_slope", L_Get_navmesh_rebake_agent_max_slope},
        {"set_navmesh_auto_rebake_agent_max_slope", L_Set_navmesh_rebake_agent_max_slope},
        {"get_navmesh_auto_rebake_cell_size", L_Get_navmesh_rebake_cell_size},
        {"set_navmesh_auto_rebake_cell_size", L_Set_navmesh_rebake_cell_size},
        {"get_navmesh_auto_rebake_cell_height", L_Get_navmesh_rebake_cell_height},
        {"set_navmesh_auto_rebake_cell_height", L_Set_navmesh_rebake_cell_height},
    });
}

} // namespace dse::runtime::lua_binding
