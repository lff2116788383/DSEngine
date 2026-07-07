/**
 * @file lua_binding_navigation.cpp
 * @brief NavMesh 寻路系统 Lua API。薄包装委托至 C ABI。
 *
 * 全局表 `nav`:
 *   nav.bake(verts, tris, config)  — 从三角面构建 navmesh
 *   nav.load(path)             — 加载 .navmesh 文件
 *   nav.save(path)             — 保存 .navmesh 文件
 *   nav.find_path(sx,sy,sz, ex,ey,ez) → {{x,y,z}, ...} | nil
 *   nav.find_nearest(x,y,z)   → x,y,z | nil
 *   nav.raycast(sx,sy,sz, ex,ey,ez) → hit, hx,hy,hz
 *   nav.is_ready()             → bool
 *
 * ECS 相关:
 *   ecs.set_nav_agent(entity, config_table)
 *   ecs.get_nav_agent(entity) → table | nil
 *   ecs.set_nav_destination(entity, x,y,z)
 *   ecs.get_nav_destination(entity) → x,y,z
 *   ecs.nav_agent_has_path(entity) → bool
 *   ecs.nav_agent_arrived(entity) → bool
 */

#ifdef DSE_ENABLE_NAVMESH

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/scripting/native_api/dse_api.h"

#include <cmath>
#include <limits>
#include <vector>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

using namespace helper;

constexpr float kKeep = std::numeric_limits<float>::quiet_NaN();

uint32_t EID(lua_State* L, int index) {
    return static_cast<uint32_t>(luaL_checkinteger(L, index));
}

// 从 table 字段读浮点，缺省返回 NaN（表示"使用默认值"）。
float OptFieldFloat(lua_State* L, int table_index, const char* field) {
    float v = kKeep;
    lua_getfield(L, table_index, field);
    if (lua_isnumber(L, -1)) v = static_cast<float>(lua_tonumber(L, -1));
    lua_pop(L, 1);
    return v;
}

// nav.is_ready() → bool
int L_NavIsReady(lua_State* L) {
    lua_pushboolean(L, dse_nav_is_ready());
    return 1;
}

// nav.load(path) → bool
int L_NavLoad(lua_State* L) {
    lua_pushboolean(L, dse_nav_load(luaL_checkstring(L, 1)));
    return 1;
}

// nav.save(path) → bool
int L_NavSave(lua_State* L) {
    lua_pushboolean(L, dse_nav_save(luaL_checkstring(L, 1)));
    return 1;
}

// nav.find_path(sx,sy,sz, ex,ey,ez) → { {x,y,z}, ... } | nil
int L_NavFindPath(lua_State* L) {
    float sx = CheckFloat(L, 1), sy = CheckFloat(L, 2), sz = CheckFloat(L, 3);
    float ex = CheckFloat(L, 4), ey = CheckFloat(L, 5), ez = CheckFloat(L, 6);
    int total = dse_nav_find_path(sx, sy, sz, ex, ey, ez, nullptr, 0);
    if (total <= 0) { lua_pushnil(L); return 1; }
    std::vector<float> xyz(static_cast<size_t>(total) * 3);
    int n = dse_nav_find_path(sx, sy, sz, ex, ey, ez, xyz.data(), total);
    if (n <= 0) { lua_pushnil(L); return 1; }
    if (n > total) n = total;
    lua_createtable(L, n, 0);
    for (int i = 0; i < n; ++i) {
        lua_createtable(L, 3, 0);
        lua_pushnumber(L, xyz[static_cast<size_t>(i) * 3 + 0]); lua_rawseti(L, -2, 1);
        lua_pushnumber(L, xyz[static_cast<size_t>(i) * 3 + 1]); lua_rawseti(L, -2, 2);
        lua_pushnumber(L, xyz[static_cast<size_t>(i) * 3 + 2]); lua_rawseti(L, -2, 3);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// nav.find_nearest(x,y,z) → x,y,z | nil
int L_NavFindNearest(lua_State* L) {
    float out[3] = {0.0f, 0.0f, 0.0f};
    if (!dse_nav_find_nearest(CheckFloat(L, 1), CheckFloat(L, 2), CheckFloat(L, 3), out)) {
        lua_pushnil(L);
        return 1;
    }
    lua_pushnumber(L, out[0]);
    lua_pushnumber(L, out[1]);
    lua_pushnumber(L, out[2]);
    return 3;
}

// nav.raycast(sx,sy,sz, ex,ey,ez) → hit(bool), hx,hy,hz
int L_NavRaycast(lua_State* L) {
    float out[3] = {0.0f, 0.0f, 0.0f};
    int blocked = dse_nav_raycast(CheckFloat(L, 1), CheckFloat(L, 2), CheckFloat(L, 3),
                                  CheckFloat(L, 4), CheckFloat(L, 5), CheckFloat(L, 6), out);
    lua_pushboolean(L, blocked);
    lua_pushnumber(L, out[0]);
    lua_pushnumber(L, out[1]);
    lua_pushnumber(L, out[2]);
    return 4;
}

// nav.bake(verts, tris, config) → bool
// arg1: verts (flat array {x0,y0,z0, x1,y1,z1, ...})
// arg2: tris  (flat array {i0,i1,i2, ...})
// arg3: config table (optional): { cell_size, cell_height, agent_height,
//                                  agent_radius, agent_max_climb, agent_max_slope }
int L_NavBake(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    int nverts_f = static_cast<int>(lua_rawlen(L, 1));
    int ntris_f = static_cast<int>(lua_rawlen(L, 2));
    int nverts = nverts_f / 3;
    int ntris = ntris_f / 3;

    std::vector<float> verts(static_cast<size_t>(nverts_f));
    for (int i = 0; i < nverts_f; ++i) {
        lua_rawgeti(L, 1, i + 1);
        verts[static_cast<size_t>(i)] = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
    }
    std::vector<int> tris(static_cast<size_t>(ntris_f));
    for (int i = 0; i < ntris_f; ++i) {
        lua_rawgeti(L, 2, i + 1);
        tris[static_cast<size_t>(i)] = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
    }

    float cell_size = kKeep, cell_height = kKeep;
    float agent_height = kKeep, agent_radius = kKeep;
    float agent_max_climb = kKeep, agent_max_slope = kKeep;
    if (lua_istable(L, 3)) {
        cell_size = OptFieldFloat(L, 3, "cell_size");
        cell_height = OptFieldFloat(L, 3, "cell_height");
        agent_height = OptFieldFloat(L, 3, "agent_height");
        agent_radius = OptFieldFloat(L, 3, "agent_radius");
        agent_max_climb = OptFieldFloat(L, 3, "agent_max_climb");
        agent_max_slope = OptFieldFloat(L, 3, "agent_max_slope");
    }

    lua_pushboolean(L, dse_nav_bake(verts.data(), nverts, tris.data(), ntris,
                                    cell_size, cell_height, agent_height, agent_radius,
                                    agent_max_climb, agent_max_slope));
    return 1;
}

// ecs.set_nav_agent(entity, config_table)
int L_EcsSetNavAgent(lua_State* L) {
    uint32_t e = EID(L, 1);
    float speed = kKeep, acceleration = kKeep, stopping_dist = kKeep;
    float radius = kKeep, height = kKeep;
    if (lua_istable(L, 2)) {
        speed = OptFieldFloat(L, 2, "speed");
        acceleration = OptFieldFloat(L, 2, "acceleration");
        stopping_dist = OptFieldFloat(L, 2, "stopping_dist");
        radius = OptFieldFloat(L, 2, "radius");
        height = OptFieldFloat(L, 2, "height");
    }
    dse_nav_agent_set(e, speed, acceleration, stopping_dist, radius, height);
    return 0;
}

// ecs.set_nav_destination(entity, x, y, z)
int L_EcsSetNavDestination(lua_State* L) {
    dse_nav_agent_set_destination(EID(L, 1), CheckFloat(L, 2), CheckFloat(L, 3), CheckFloat(L, 4));
    return 0;
}

// ecs.get_nav_agent(entity) → table | nil
int L_EcsGetNavAgent(lua_State* L) {
    float params[8] = {0};
    int flags[4] = {0};
    if (!dse_nav_agent_get(EID(L, 1), params, flags)) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, 0, 12);
    lua_pushnumber(L, params[0]); lua_setfield(L, -2, "speed");
    lua_pushnumber(L, params[1]); lua_setfield(L, -2, "acceleration");
    lua_pushnumber(L, params[2]); lua_setfield(L, -2, "stopping_dist");
    lua_pushnumber(L, params[3]); lua_setfield(L, -2, "radius");
    lua_pushnumber(L, params[4]); lua_setfield(L, -2, "height");
    lua_pushnumber(L, params[5]); lua_setfield(L, -2, "dest_x");
    lua_pushnumber(L, params[6]); lua_setfield(L, -2, "dest_y");
    lua_pushnumber(L, params[7]); lua_setfield(L, -2, "dest_z");
    lua_pushboolean(L, flags[0]); lua_setfield(L, -2, "has_path");
    lua_pushboolean(L, flags[1]); lua_setfield(L, -2, "path_pending");
    lua_pushboolean(L, flags[2]); lua_setfield(L, -2, "arrived");
    lua_pushinteger(L, flags[3]); lua_setfield(L, -2, "current_waypoint");
    return 1;
}

// ecs.get_nav_destination(entity) → x, y, z
int L_EcsGetNavDestination(lua_State* L) {
    float out[3] = {0.0f, 0.0f, 0.0f};
    dse_nav_agent_get_destination(EID(L, 1), out);
    lua_pushnumber(L, out[0]);
    lua_pushnumber(L, out[1]);
    lua_pushnumber(L, out[2]);
    return 3;
}

// ecs.nav_agent_has_path(entity) → bool
int L_EcsNavAgentHasPath(lua_State* L) {
    lua_pushboolean(L, dse_nav_agent_has_path(EID(L, 1)));
    return 1;
}

// ecs.nav_agent_arrived(entity) → bool
int L_EcsNavAgentArrived(lua_State* L) {
    lua_pushboolean(L, dse_nav_agent_arrived(EID(L, 1)));
    return 1;
}

} // namespace

void RegisterNavigationBindings(lua_State* L) {
    // 全局 nav 表
    lua_newtable(L);
    lua_pushcfunction(L, L_NavIsReady);   lua_setfield(L, -2, "is_ready");
    lua_pushcfunction(L, L_NavLoad);      lua_setfield(L, -2, "load");
    lua_pushcfunction(L, L_NavSave);      lua_setfield(L, -2, "save");
    lua_pushcfunction(L, L_NavFindPath);  lua_setfield(L, -2, "find_path");
    lua_pushcfunction(L, L_NavFindNearest); lua_setfield(L, -2, "find_nearest");
    lua_pushcfunction(L, L_NavRaycast);   lua_setfield(L, -2, "raycast");
    lua_pushcfunction(L, L_NavBake);      lua_setfield(L, -2, "bake");
    lua_setglobal(L, "nav");

    // ECS nav agent 注册到 dse.ecs 表
    lua_getglobal(L, "dse");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "ecs");
        if (lua_istable(L, -1)) {
            lua_pushcfunction(L, L_EcsSetNavAgent);         lua_setfield(L, -2, "set_nav_agent");
            lua_pushcfunction(L, L_EcsGetNavAgent);         lua_setfield(L, -2, "get_nav_agent");
            lua_pushcfunction(L, L_EcsSetNavDestination);   lua_setfield(L, -2, "set_nav_destination");
            lua_pushcfunction(L, L_EcsGetNavDestination);   lua_setfield(L, -2, "get_nav_destination");
            lua_pushcfunction(L, L_EcsNavAgentHasPath);     lua_setfield(L, -2, "nav_agent_has_path");
            lua_pushcfunction(L, L_EcsNavAgentArrived);     lua_setfield(L, -2, "nav_agent_arrived");
        }
        lua_pop(L, 1); // pop ecs
    }
    lua_pop(L, 1); // pop dse
}

} // namespace dse::runtime::lua_binding

#endif // DSE_ENABLE_NAVMESH
