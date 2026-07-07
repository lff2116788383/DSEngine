/**
 * @file lua_binding_streaming.cpp
 * @brief Lua 绑定：资源流式加载系统 (dse.streaming)
 *
 * 薄包装：仅做 Lua 参数读取与结果入栈，所有逻辑委托 C ABI（dse_api_extended.cpp）。
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/native_api/dse_api.h"

#include <vector>
#include <string>

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

namespace {

static const char* ZoneStateToStr(int state) {
    switch (state) {
    case 0: return "unloaded";
    case 1: return "loading";
    case 2: return "loaded";
    case 3: return "unloading";
    default: return "unloaded";
    }
}

int L_StreamingCreateZone(lua_State* L) {
    const char* name = luaL_checkstring(L, 1);
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    float load_r = static_cast<float>(luaL_checknumber(L, 5));
    float unload_r = static_cast<float>(luaL_optnumber(L, 6, load_r * 1.5));
    lua_pushinteger(L, static_cast<lua_Integer>(dse_streaming_create_zone(name, cx, cy, cz, load_r, unload_r)));
    return 1;
}

int L_StreamingDestroyZone(lua_State* L) {
    dse_streaming_destroy_zone(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

int L_StreamingAddAsset(lua_State* L) {
    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    const char* type_str = luaL_optstring(L, 3, "texture");
    dse_streaming_add_asset(zone_id, path, type_str);
    return 0;
}

int L_StreamingAddAssets(lua_State* L) {
    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    const char* type_str = luaL_optstring(L, 3, "texture");
    int n = static_cast<int>(lua_rawlen(L, 2));
    std::vector<std::string> paths_str;
    std::vector<const char*> paths;
    paths_str.reserve(n);
    paths.reserve(n);
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        if (lua_isstring(L, -1)) {
            paths_str.emplace_back(lua_tostring(L, -1));
            paths.push_back(paths_str.back().c_str());
        }
        lua_pop(L, 1);
    }
    dse_streaming_add_assets(zone_id, paths.data(), static_cast<int>(paths.size()), type_str);
    return 0;
}

int L_StreamingSetZoneCenter(lua_State* L) {
    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    dse_streaming_set_zone_center(zone_id, cx, cy, cz);
    return 0;
}

int L_StreamingForceLoad(lua_State* L) {
    dse_streaming_force_load(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

int L_StreamingForceUnload(lua_State* L) {
    dse_streaming_force_unload(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    return 0;
}

int L_StreamingGetZoneState(lua_State* L) {
    int state = dse_streaming_get_zone_state(static_cast<uint32_t>(luaL_checkinteger(L, 1)));
    lua_pushstring(L, ZoneStateToStr(state));
    return 1;
}

int L_StreamingGetZoneProgress(lua_State* L) {
    lua_pushnumber(L, dse_streaming_get_zone_progress(static_cast<uint32_t>(luaL_checkinteger(L, 1))));
    return 1;
}

int L_StreamingSetBudget(lua_State* L) {
    int per_frame = static_cast<int>(luaL_optinteger(L, 1, 8));
    int max_concurrent = static_cast<int>(luaL_optinteger(L, 2, 32));
    dse_streaming_set_budget(per_frame, max_concurrent);
    return 0;
}

int L_StreamingGetActiveLoads(lua_State* L) {
    lua_pushinteger(L, dse_streaming_get_active_loads());
    return 1;
}

int L_StreamingGetZoneCount(lua_State* L) {
    lua_pushinteger(L, dse_streaming_get_zone_count());
    return 1;
}

} // anonymous namespace

void RegisterStreamingBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };

    lua_newtable(L);
    set_fn("create_zone", L_StreamingCreateZone);
    set_fn("destroy_zone", L_StreamingDestroyZone);
    set_fn("add_asset", L_StreamingAddAsset);
    set_fn("add_assets", L_StreamingAddAssets);
    set_fn("set_zone_center", L_StreamingSetZoneCenter);
    set_fn("force_load", L_StreamingForceLoad);
    set_fn("force_unload", L_StreamingForceUnload);
    set_fn("get_zone_state", L_StreamingGetZoneState);
    set_fn("get_zone_progress", L_StreamingGetZoneProgress);
    set_fn("set_budget", L_StreamingSetBudget);
    set_fn("get_active_loads", L_StreamingGetActiveLoads);
    set_fn("get_zone_count", L_StreamingGetZoneCount);
    lua_setglobal(L, "streaming");
}

} // namespace dse::runtime::lua_binding
