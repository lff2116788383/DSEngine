/**
 * @file lua_binding_navigation.cpp
 * @brief NavMesh 寻路系统 Lua API
 *
 * 全局表 `nav`:
 *   nav.bake(config)           — 从场景三角面构建 navmesh
 *   nav.load(path)             — 加载 .navmesh 文件
 *   nav.save(path)             — 保存 .navmesh 文件
 *   nav.find_path(sx,sy,sz, ex,ey,ez) → {{x,y,z}, ...} | nil
 *   nav.find_nearest(x,y,z)   → x,y,z | nil
 *   nav.raycast(sx,sy,sz, ex,ey,ez) → hit, hx,hy,hz
 *   nav.is_ready()             → bool
 *
 * ECS 相关:
 *   ecs.set_nav_agent(entity, config_table)
 *   ecs.set_nav_destination(entity, x,y,z)
 *   ecs.nav_agent_arrived(entity) → bool
 */

#ifdef DSE_ENABLE_NAVMESH

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_helper.h"
#include "engine/navigation/nav_mesh_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/core/service_locator.h"
#include <vector>
#include <glm/glm.hpp>

extern "C" {
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {
namespace {

using namespace helper;

navigation::NavMeshSystem* GetNav() {
    return core::ServiceLocator::Instance().Get<navigation::NavMeshSystem>();
}

// nav.is_ready() → bool
int L_NavIsReady(lua_State* L) {
    auto* nav = GetNav();
    lua_pushboolean(L, nav && nav->IsReady());
    return 1;
}

// nav.load(path) → bool
int L_NavLoad(lua_State* L) {
    auto* nav = GetNav();
    if (!nav) { lua_pushboolean(L, false); return 1; }
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, nav->LoadNavMesh(path));
    return 1;
}

// nav.save(path) → bool
int L_NavSave(lua_State* L) {
    auto* nav = GetNav();
    if (!nav) { lua_pushboolean(L, false); return 1; }
    const char* path = luaL_checkstring(L, 1);
    lua_pushboolean(L, nav->SaveNavMesh(path));
    return 1;
}

// nav.find_path(sx,sy,sz, ex,ey,ez) → { {x,y,z}, ... } | nil
int L_NavFindPath(lua_State* L) {
    auto* nav = GetNav();
    if (!nav || !nav->IsReady()) { lua_pushnil(L); return 1; }
    glm::vec3 start(CheckFloat(L,1), CheckFloat(L,2), CheckFloat(L,3));
    glm::vec3 end(CheckFloat(L,4), CheckFloat(L,5), CheckFloat(L,6));
    std::vector<glm::vec3> path;
    if (!nav->FindPath(start, end, path) || path.empty()) {
        lua_pushnil(L);
        return 1;
    }
    lua_createtable(L, (int)path.size(), 0);
    for (int i = 0; i < (int)path.size(); ++i) {
        lua_createtable(L, 3, 0);
        lua_pushnumber(L, path[i].x); lua_rawseti(L, -2, 1);
        lua_pushnumber(L, path[i].y); lua_rawseti(L, -2, 2);
        lua_pushnumber(L, path[i].z); lua_rawseti(L, -2, 3);
        lua_rawseti(L, -2, i + 1);
    }
    return 1;
}

// nav.find_nearest(x,y,z) → x,y,z | nil
int L_NavFindNearest(lua_State* L) {
    auto* nav = GetNav();
    if (!nav || !nav->IsReady()) { lua_pushnil(L); return 1; }
    glm::vec3 pos(CheckFloat(L,1), CheckFloat(L,2), CheckFloat(L,3));
    glm::vec3 nearest;
    if (!nav->FindNearestPoint(pos, nearest)) { lua_pushnil(L); return 1; }
    lua_pushnumber(L, nearest.x);
    lua_pushnumber(L, nearest.y);
    lua_pushnumber(L, nearest.z);
    return 3;
}

// nav.raycast(sx,sy,sz, ex,ey,ez) → hit(bool), hx,hy,hz
int L_NavRaycast(lua_State* L) {
    auto* nav = GetNav();
    if (!nav || !nav->IsReady()) {
        lua_pushboolean(L, false);
        lua_pushnumber(L, 0); lua_pushnumber(L, 0); lua_pushnumber(L, 0);
        return 4;
    }
    glm::vec3 start(CheckFloat(L,1), CheckFloat(L,2), CheckFloat(L,3));
    glm::vec3 end(CheckFloat(L,4), CheckFloat(L,5), CheckFloat(L,6));
    glm::vec3 hit;
    bool blocked = nav->Raycast(start, end, hit);
    lua_pushboolean(L, blocked);
    lua_pushnumber(L, hit.x);
    lua_pushnumber(L, hit.y);
    lua_pushnumber(L, hit.z);
    return 4;
}

// nav.bake(config_table) → bool
// config_table: { cell_size, cell_height, agent_height, agent_radius, agent_max_climb, agent_max_slope }
// 注意：BakeFromTriangles 需要外部提供三角面数据；此处暂提供 bake_from_data(verts_flat, tris_flat, config)
int L_NavBake(lua_State* L) {
    auto* nav = GetNav();
    if (!nav) { lua_pushboolean(L, false); return 1; }
    // arg1: verts (flat array {x0,y0,z0, x1,y1,z1, ...})
    // arg2: tris  (flat array {i0,i1,i2, ...})
    // arg3: config table (optional)
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TTABLE);

    int nverts_f = (int)lua_rawlen(L, 1);
    int ntris_f  = (int)lua_rawlen(L, 2);
    int nverts   = nverts_f / 3;
    int ntris    = ntris_f  / 3;

    std::vector<float> verts(nverts_f);
    for (int i = 0; i < nverts_f; ++i) {
        lua_rawgeti(L, 1, i + 1);
        verts[i] = (float)lua_tonumber(L, -1);
        lua_pop(L, 1);
    }
    std::vector<int> tris(ntris_f);
    for (int i = 0; i < ntris_f; ++i) {
        lua_rawgeti(L, 2, i + 1);
        tris[i] = (int)lua_tointeger(L, -1);
        lua_pop(L, 1);
    }

    navigation::NavMeshBuildConfig cfg{};
    if (lua_istable(L, 3)) {
        lua_getfield(L, 3, "cell_size");       if (!lua_isnil(L,-1)) cfg.cell_size       = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 3, "cell_height");     if (!lua_isnil(L,-1)) cfg.cell_height     = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 3, "agent_height");    if (!lua_isnil(L,-1)) cfg.agent_height    = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 3, "agent_radius");    if (!lua_isnil(L,-1)) cfg.agent_radius    = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 3, "agent_max_climb"); if (!lua_isnil(L,-1)) cfg.agent_max_climb = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 3, "agent_max_slope"); if (!lua_isnil(L,-1)) cfg.agent_max_slope = (float)lua_tonumber(L,-1); lua_pop(L,1);
    }

    bool ok = nav->BakeFromTriangles(verts.data(), nverts, tris.data(), ntris, cfg);
    lua_pushboolean(L, ok);
    return 1;
}

// ecs.set_nav_agent(entity, config_table)
int L_EcsSetNavAgent(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e)) return 0;
    auto& agent = world->registry().emplace_or_replace<NavMeshAgentComponent>(e);
    if (lua_istable(L, 2)) {
        lua_getfield(L, 2, "speed");         if (!lua_isnil(L,-1)) agent.speed         = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 2, "acceleration");  if (!lua_isnil(L,-1)) agent.acceleration  = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 2, "stopping_dist"); if (!lua_isnil(L,-1)) agent.stopping_dist = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 2, "radius");        if (!lua_isnil(L,-1)) agent.agent_radius  = (float)lua_tonumber(L,-1); lua_pop(L,1);
        lua_getfield(L, 2, "height");        if (!lua_isnil(L,-1)) agent.agent_height  = (float)lua_tonumber(L,-1); lua_pop(L,1);
    }
    return 0;
}

// ecs.set_nav_destination(entity, x, y, z)
int L_EcsSetNavDestination(lua_State* L) {
    World* world = GetWorld();
    if (!world) return 0;
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<NavMeshAgentComponent>(e)) return 0;
    auto& agent = world->registry().get<NavMeshAgentComponent>(e);
    agent.destination = glm::vec3(CheckFloat(L,2), CheckFloat(L,3), CheckFloat(L,4));
    agent.path_pending = true;
    agent.arrived = false;
    return 0;
}

// ecs.nav_agent_arrived(entity) → bool
int L_EcsNavAgentArrived(lua_State* L) {
    World* world = GetWorld();
    if (!world) { lua_pushboolean(L, true); return 1; }
    Entity e = LuaEntityFromInteger(luaL_checkinteger(L, 1));
    if (!world->registry().valid(e) || !world->registry().all_of<NavMeshAgentComponent>(e)) {
        lua_pushboolean(L, true);
        return 1;
    }
    auto& agent = world->registry().get<NavMeshAgentComponent>(e);
    lua_pushboolean(L, agent.arrived);
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
            lua_pushcfunction(L, L_EcsSetNavAgent);       lua_setfield(L, -2, "set_nav_agent");
            lua_pushcfunction(L, L_EcsSetNavDestination); lua_setfield(L, -2, "set_nav_destination");
            lua_pushcfunction(L, L_EcsNavAgentArrived);   lua_setfield(L, -2, "nav_agent_arrived");
        }
        lua_pop(L, 1); // pop ecs
    }
    lua_pop(L, 1); // pop dse
}

} // namespace dse::runtime::lua_binding

#endif // DSE_ENABLE_NAVMESH
