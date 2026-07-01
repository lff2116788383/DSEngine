/**
 * @file lua_binding_ecs_world_partition.gen.cpp
 * @brief 自动生成 — 勿手动修改
 *        来源：tools/codegen/binding_defs.json
 *
 * WorldPartitionConfigComponent 的 Lua 绑定，内部委托调用 dse_api C ABI 层。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/native_api/dse_api.h"
extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

int L_Get_world_partition_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushboolean(L, dse_world_partition_get_enabled(e));
    return 1;
}
int L_Set_world_partition_enabled(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_enabled(e, lua_toboolean(L, 2));
    return 0;
}
int L_Get_world_partition_cell_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, dse_world_partition_get_cell_size(e));
    return 1;
}
int L_Set_world_partition_cell_size(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_cell_size(e, static_cast<float>(luaL_checknumber(L, 2)));
    return 0;
}
int L_Get_world_partition_cells_directory(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    char buf[512];
    if (dse_world_partition_get_cells_directory(e, buf, 512) <= 0) {
        lua_pushstring(L, "");
        return 1;
    }
    lua_pushstring(L, buf);
    return 1;
}
int L_Set_world_partition_cells_directory(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_cells_directory(e, luaL_checkstring(L, 2));
    return 0;
}
int L_Get_world_partition_grid_min_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_world_partition_get_grid_min_x(e));
    return 1;
}
int L_Set_world_partition_grid_min_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_grid_min_x(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_world_partition_grid_max_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_world_partition_get_grid_max_x(e));
    return 1;
}
int L_Set_world_partition_grid_max_x(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_grid_max_x(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_world_partition_grid_min_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_world_partition_get_grid_min_y(e));
    return 1;
}
int L_Set_world_partition_grid_min_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_grid_min_y(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_world_partition_grid_max_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_world_partition_get_grid_max_y(e));
    return 1;
}
int L_Set_world_partition_grid_max_y(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_grid_max_y(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}
int L_Get_world_partition_max_loads_per_frame(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushinteger(L, dse_world_partition_get_max_loads_per_frame(e));
    return 1;
}
int L_Set_world_partition_max_loads_per_frame(lua_State* L) {
    uint32_t e = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    dse_world_partition_set_max_loads_per_frame(e, static_cast<int>(luaL_checkinteger(L, 2)));
    return 0;
}

} // namespace

void RegisterWorldPartitionConfigComponentGenBindings(lua_State* L) {
    using namespace helper;
    RegisterBindings(L, {
        {"get_world_partition_enabled", L_Get_world_partition_enabled},
        {"set_world_partition_enabled", L_Set_world_partition_enabled},
        {"get_world_partition_cell_size", L_Get_world_partition_cell_size},
        {"set_world_partition_cell_size", L_Set_world_partition_cell_size},
        {"get_world_partition_cells_directory", L_Get_world_partition_cells_directory},
        {"set_world_partition_cells_directory", L_Set_world_partition_cells_directory},
        {"get_world_partition_grid_min_x", L_Get_world_partition_grid_min_x},
        {"set_world_partition_grid_min_x", L_Set_world_partition_grid_min_x},
        {"get_world_partition_grid_max_x", L_Get_world_partition_grid_max_x},
        {"set_world_partition_grid_max_x", L_Set_world_partition_grid_max_x},
        {"get_world_partition_grid_min_y", L_Get_world_partition_grid_min_y},
        {"set_world_partition_grid_min_y", L_Set_world_partition_grid_min_y},
        {"get_world_partition_grid_max_y", L_Get_world_partition_grid_max_y},
        {"set_world_partition_grid_max_y", L_Set_world_partition_grid_max_y},
        {"get_world_partition_max_loads_per_frame", L_Get_world_partition_max_loads_per_frame},
        {"set_world_partition_max_loads_per_frame", L_Set_world_partition_max_loads_per_frame},
    });
}

} // namespace dse::runtime::lua_binding
