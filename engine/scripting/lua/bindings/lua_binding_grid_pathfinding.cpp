/**
 * @file lua_binding_grid_pathfinding.cpp
 * @brief Lua 绑定：2D 网格寻路 (dse.pathfinding) — A* 算法
 *
 * API:
 *   -- 创建寻路器
 *   local pf = dse.pathfinding.create()
 *
 *   -- 从网格数据构建
 *   pf:build_from_grid(width, height, cell_size, walkable_table)
 *   -- walkable_table: {true, false, true, ...} true=可走, false=障碍
 *
 *   -- 从 Tilemap 数据构建
 *   pf:build_from_tilemap(width, height, cell_size, tiles_table, blocked_ids_table)
 *   -- tiles_table: {1, 0, 2, 3, ...} tile_id, 0=空/障碍
 *   -- blocked_ids_table: 可选, {2, 5} 表示 tile_id 2和5也是障碍
 *
 *   -- 动态障碍
 *   pf:set_blocked(x, y, true)      -- 设置障碍
 *   pf:is_blocked(x, y)             -- 查询
 *   pf:is_blocked_at(world_x, world_y)
 *   pf:clear_blocks()               -- 清除所有障碍
 *
 *   -- 寻路
 *   local path = pf:find_path(sx, sy, ex, ey [, opts])
 *   -- opts: { mode=4|8, diagonal="no_corner"|"always"|"never" }
 *   -- path: { {x=, y=}, {x=, y=}, ... } 世界坐标点列表
 *   -- 返回 nil 表示无路径
 *
 *   -- 网格坐标寻路
 *   local grid_path = pf:find_path_grid(sx, sy, ex, ey [, opts])
 *   -- grid_path: { {x=, y=}, ... } 网格坐标点列表
 *
 *   -- 坐标转换
 *   local gx, gy = pf:world_to_grid(world_x, world_y)
 *   local wx, wy = pf:grid_to_world(gx, gy)
 *
 *   -- 查询
 *   pf:is_ready()
 *   pf:get_width()
 *   pf:get_height()
 *   pf:get_cell_size()
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

#include "engine/navigation/grid_pathfinding.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace dse::runtime::lua_binding {
namespace {

using dse::navigation::GridPathfinding;
using dse::navigation::GridPathConfig;
using dse::navigation::GridMoveMode;
using dse::navigation::GridDiagonalPolicy;

static const char* kMetatableName = "dse_grid_pathfinding";

static GridPathfinding* ToPathfinder(lua_State* L, int idx) {
    return *static_cast<GridPathfinding**>(luaL_checkudata(L, idx, kMetatableName));
}

// ── 生命周期 ──────────────────────────────────────────────────────────────

static int L_Create(lua_State* L) {
    auto** ud = static_cast<GridPathfinding**>(
        lua_newuserdata(L, sizeof(GridPathfinding*)));
    *ud = new GridPathfinding();
    luaL_setmetatable(L, kMetatableName);
    return 1;
}

static int L_GC(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    delete pf;
    return 0;
}

// ── 构建 ──────────────────────────────────────────────────────────────────

static int L_BuildFromGrid(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    int width = static_cast<int>(luaL_checkinteger(L, 2));
    int height = static_cast<int>(luaL_checkinteger(L, 3));
    float cell_size = static_cast<float>(luaL_checknumber(L, 4));
    luaL_checktype(L, 5, LUA_TTABLE);

    std::vector<bool> walkable;
    size_t len = lua_rawlen(L, 5);
    walkable.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, 5, static_cast<lua_Integer>(i));
        walkable.push_back(lua_toboolean(L, -1) != 0);
        lua_pop(L, 1);
    }

    pf->BuildFromGrid(width, height, cell_size, walkable);
    return 0;
}

static int L_BuildFromTilemap(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    int width = static_cast<int>(luaL_checkinteger(L, 2));
    int height = static_cast<int>(luaL_checkinteger(L, 3));
    float cell_size = static_cast<float>(luaL_checknumber(L, 4));
    luaL_checktype(L, 5, LUA_TTABLE);

    std::vector<int> tiles;
    size_t len = lua_rawlen(L, 5);
    tiles.reserve(len);
    for (size_t i = 1; i <= len; ++i) {
        lua_rawgeti(L, 5, static_cast<lua_Integer>(i));
        tiles.push_back(static_cast<int>(luaL_checkinteger(L, -1)));
        lua_pop(L, 1);
    }

    std::vector<int> blocked_ids;
    if (lua_gettop(L) >= 6 && lua_type(L, 6) == LUA_TTABLE) {
        size_t blen = lua_rawlen(L, 6);
        blocked_ids.reserve(blen);
        for (size_t i = 1; i <= blen; ++i) {
            lua_rawgeti(L, 6, static_cast<lua_Integer>(i));
            blocked_ids.push_back(static_cast<int>(luaL_checkinteger(L, -1)));
            lua_pop(L, 1);
        }
    }

    pf->BuildFromTilemap(width, height, cell_size,
                         tiles.data(), static_cast<int>(tiles.size()),
                         blocked_ids.empty() ? nullptr : blocked_ids.data(),
                         static_cast<int>(blocked_ids.size()));
    return 0;
}

// ── 动态障碍 ──────────────────────────────────────────────────────────────

static int L_SetBlocked(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));
    bool blocked = lua_toboolean(L, 4) != 0;
    pf->SetBlocked(x, y, blocked);
    return 0;
}

static int L_IsBlocked(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    int x = static_cast<int>(luaL_checkinteger(L, 2));
    int y = static_cast<int>(luaL_checkinteger(L, 3));
    lua_pushboolean(L, pf->IsBlocked(x, y) ? 1 : 0);
    return 1;
}

static int L_IsBlockedAt(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    lua_pushboolean(L, pf->IsBlockedAt({wx, wy}) ? 1 : 0);
    return 1;
}

static int L_ClearBlocks(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    pf->ClearDynamicBlocks();
    return 0;
}

// ── 配置解析 ──────────────────────────────────────────────────────────────

static GridPathConfig ParseConfig(lua_State* L, int idx) {
    GridPathConfig config;
    if (lua_type(L, idx) != LUA_TTABLE) return config;

    lua_getfield(L, idx, "mode");
    if (lua_isinteger(L, -1)) {
        int mode = static_cast<int>(lua_tointeger(L, -1));
        if (mode == 4) config.move_mode = GridMoveMode::Four;
    } else if (lua_isstring(L, -1)) {
        const char* s = lua_tostring(L, -1);
        if (std::strcmp(s, "four") == 0 || std::strcmp(s, "4") == 0)
            config.move_mode = GridMoveMode::Four;
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "diagonal");
    if (lua_isstring(L, -1)) {
        const char* s = lua_tostring(L, -1);
        if (std::strcmp(s, "always") == 0)
            config.diagonal_policy = GridDiagonalPolicy::Always;
        else if (std::strcmp(s, "never") == 0)
            config.diagonal_policy = GridDiagonalPolicy::Never;
        else  // "no_corner"
            config.diagonal_policy = GridDiagonalPolicy::NoCorner;
    }
    lua_pop(L, 1);

    lua_getfield(L, idx, "allow_diagonal");
    if (lua_isboolean(L, -1))
        config.allow_diagonal = lua_toboolean(L, -1) != 0;
    lua_pop(L, 1);

    return config;
}

// ── 寻路 ──────────────────────────────────────────────────────────────────

static int L_FindPath(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    float sx = static_cast<float>(luaL_checknumber(L, 2));
    float sy = static_cast<float>(luaL_checknumber(L, 3));
    float ex = static_cast<float>(luaL_checknumber(L, 4));
    float ey = static_cast<float>(luaL_checknumber(L, 5));

    GridPathConfig config;
    if (lua_gettop(L) >= 6 && lua_type(L, 6) == LUA_TTABLE)
        config = ParseConfig(L, 6);

    std::vector<glm::vec2> path;
    if (!pf->FindPath({sx, sy}, {ex, ey}, path, config)) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    for (size_t i = 0; i < path.size(); ++i) {
        lua_newtable(L);
        lua_pushnumber(L, path[i].x);
        lua_setfield(L, -2, "x");
        lua_pushnumber(L, path[i].y);
        lua_setfield(L, -2, "y");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

static int L_FindPathGrid(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    int sx = static_cast<int>(luaL_checkinteger(L, 2));
    int sy = static_cast<int>(luaL_checkinteger(L, 3));
    int ex = static_cast<int>(luaL_checkinteger(L, 4));
    int ey = static_cast<int>(luaL_checkinteger(L, 5));

    GridPathConfig config;
    if (lua_gettop(L) >= 6 && lua_type(L, 6) == LUA_TTABLE)
        config = ParseConfig(L, 6);

    std::vector<glm::ivec2> grid_path;
    if (!pf->FindPathGrid(sx, sy, ex, ey, grid_path, config)) {
        lua_pushnil(L);
        return 1;
    }

    lua_newtable(L);
    for (size_t i = 0; i < grid_path.size(); ++i) {
        lua_newtable(L);
        lua_pushinteger(L, grid_path[i].x);
        lua_setfield(L, -2, "x");
        lua_pushinteger(L, grid_path[i].y);
        lua_setfield(L, -2, "y");
        lua_rawseti(L, -2, static_cast<lua_Integer>(i + 1));
    }
    return 1;
}

// ── 坐标转换 ──────────────────────────────────────────────────────────────

static int L_WorldToGrid(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    float wx = static_cast<float>(luaL_checknumber(L, 2));
    float wy = static_cast<float>(luaL_checknumber(L, 3));
    glm::ivec2 g = pf->WorldToGrid({wx, wy});
    lua_pushinteger(L, g.x);
    lua_pushinteger(L, g.y);
    return 2;
}

static int L_GridToWorld(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    int gx = static_cast<int>(luaL_checkinteger(L, 2));
    int gy = static_cast<int>(luaL_checkinteger(L, 3));
    glm::vec2 w = pf->GridToWorld(gx, gy);
    lua_pushnumber(L, w.x);
    lua_pushnumber(L, w.y);
    return 2;
}

// ── 查询 ──────────────────────────────────────────────────────────────────

static int L_IsReady(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    lua_pushboolean(L, pf->IsReady() ? 1 : 0);
    return 1;
}

static int L_GetWidth(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    lua_pushinteger(L, pf->GetWidth());
    return 1;
}

static int L_GetHeight(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    lua_pushinteger(L, pf->GetHeight());
    return 1;
}

static int L_GetCellSize(lua_State* L) {
    auto* pf = ToPathfinder(L, 1);
    lua_pushnumber(L, pf->GetCellSize());
    return 1;
}

static const luaL_Reg kMethods[] = {
    {"build_from_grid",    L_BuildFromGrid},
    {"build_from_tilemap", L_BuildFromTilemap},
    {"set_blocked",        L_SetBlocked},
    {"is_blocked",         L_IsBlocked},
    {"is_blocked_at",      L_IsBlockedAt},
    {"clear_blocks",       L_ClearBlocks},
    {"find_path",          L_FindPath},
    {"find_path_grid",     L_FindPathGrid},
    {"world_to_grid",      L_WorldToGrid},
    {"grid_to_world",      L_GridToWorld},
    {"is_ready",           L_IsReady},
    {"get_width",          L_GetWidth},
    {"get_height",         L_GetHeight},
    {"get_cell_size",      L_GetCellSize},
    {nullptr, nullptr}
};

} // anonymous namespace

void RegisterGridPathfindingBindings(lua_State* L) {
    // 创建 metatable
    luaL_newmetatable(L, kMetatableName);
    lua_newtable(L);  // methods
    luaL_setfuncs(L, kMethods, 0);
    lua_setfield(L, -2, "__index");
    lua_pushcfunction(L, L_GC);
    lua_setfield(L, -2, "__gc");
    lua_pop(L, 1);

    // dse.pathfinding 模块表
    lua_newtable(L);
    lua_pushcfunction(L, L_Create);
    lua_setfield(L, -2, "create");
}

} // namespace dse::runtime::lua_binding
