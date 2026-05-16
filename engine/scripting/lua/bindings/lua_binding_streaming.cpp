/**
 * @file lua_binding_streaming.cpp
 * @brief Lua 绑定：资源流式加载系统 (dse.streaming)
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/assets/streaming_manager.h"
#include "engine/core/service_locator.h"

extern "C" {
#include "depends/lua/lua.h"
#include "depends/lua/lauxlib.h"
}

namespace dse::runtime::lua_binding {

namespace {

dse::streaming::StreamingManager* GetStreamingManager() {
    return dse::core::ServiceLocator::Instance().Get<dse::streaming::StreamingManager>();
}

dse::streaming::AssetType ParseAssetType(const char* type_str) {
    if (!type_str) return dse::streaming::AssetType::Texture;
    if (strcmp(type_str, "texture") == 0) return dse::streaming::AssetType::Texture;
    if (strcmp(type_str, "mesh") == 0) return dse::streaming::AssetType::Mesh;
    if (strcmp(type_str, "animation") == 0) return dse::streaming::AssetType::Animation;
    if (strcmp(type_str, "skeleton") == 0) return dse::streaming::AssetType::Skeleton;
    if (strcmp(type_str, "audio") == 0) return dse::streaming::AssetType::Audio;
    if (strcmp(type_str, "material") == 0) return dse::streaming::AssetType::Material;
    return dse::streaming::AssetType::Texture;
}

// streaming.create_zone(name, cx, cy, cz, load_radius, unload_radius) → zone_id
int L_StreamingCreateZone(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) { lua_pushinteger(L, 0); return 1; }

    const char* name = luaL_checkstring(L, 1);
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));
    float load_r = static_cast<float>(luaL_checknumber(L, 5));
    float unload_r = static_cast<float>(luaL_optnumber(L, 6, load_r * 1.5));

    uint32_t id = mgr->CreateZone(name, glm::vec3(cx, cy, cz), load_r, unload_r);
    lua_pushinteger(L, static_cast<lua_Integer>(id));
    return 1;
}

// streaming.destroy_zone(zone_id)
int L_StreamingDestroyZone(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    mgr->DestroyZone(zone_id);
    return 0;
}

// streaming.add_asset(zone_id, path, type_string)
int L_StreamingAddAsset(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    const char* path = luaL_checkstring(L, 2);
    const char* type_str = luaL_optstring(L, 3, "texture");

    mgr->AddAsset(zone_id, path, ParseAssetType(type_str));
    return 0;
}

// streaming.add_assets(zone_id, {path1, path2, ...}, type_string)
int L_StreamingAddAssets(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    luaL_checktype(L, 2, LUA_TTABLE);
    const char* type_str = luaL_optstring(L, 3, "texture");
    auto type = ParseAssetType(type_str);

    std::vector<std::string> paths;
    int n = static_cast<int>(lua_rawlen(L, 2));
    paths.reserve(static_cast<size_t>(n));
    for (int i = 1; i <= n; ++i) {
        lua_rawgeti(L, 2, i);
        if (lua_isstring(L, -1)) {
            paths.emplace_back(lua_tostring(L, -1));
        }
        lua_pop(L, 1);
    }

    mgr->AddAssets(zone_id, paths, type);
    return 0;
}

// streaming.set_zone_center(zone_id, cx, cy, cz)
int L_StreamingSetZoneCenter(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    float cx = static_cast<float>(luaL_checknumber(L, 2));
    float cy = static_cast<float>(luaL_checknumber(L, 3));
    float cz = static_cast<float>(luaL_checknumber(L, 4));

    mgr->SetZoneCenter(zone_id, glm::vec3(cx, cy, cz));
    return 0;
}

// streaming.force_load(zone_id)
int L_StreamingForceLoad(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    mgr->ForceLoadZone(zone_id);
    return 0;
}

// streaming.force_unload(zone_id)
int L_StreamingForceUnload(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    mgr->ForceUnloadZone(zone_id);
    return 0;
}

// streaming.get_zone_state(zone_id) → "unloaded"|"loading"|"loaded"|"unloading"
int L_StreamingGetZoneState(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) { lua_pushstring(L, "unloaded"); return 1; }

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    auto state = mgr->GetZoneState(zone_id);

    switch (state) {
    case dse::streaming::ZoneState::Unloaded:  lua_pushstring(L, "unloaded"); break;
    case dse::streaming::ZoneState::Loading:   lua_pushstring(L, "loading"); break;
    case dse::streaming::ZoneState::Loaded:    lua_pushstring(L, "loaded"); break;
    case dse::streaming::ZoneState::Unloading: lua_pushstring(L, "unloading"); break;
    default: lua_pushstring(L, "unloaded"); break;
    }
    return 1;
}

// streaming.get_zone_progress(zone_id) → float [0.0, 1.0]
int L_StreamingGetZoneProgress(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) { lua_pushnumber(L, 0.0); return 1; }

    uint32_t zone_id = static_cast<uint32_t>(luaL_checkinteger(L, 1));
    lua_pushnumber(L, static_cast<lua_Number>(mgr->GetZoneProgress(zone_id)));
    return 1;
}

// streaming.set_budget(per_frame, max_concurrent)
int L_StreamingSetBudget(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) return 0;

    int per_frame = static_cast<int>(luaL_optinteger(L, 1, 8));
    int max_concurrent = static_cast<int>(luaL_optinteger(L, 2, 32));

    mgr->SetLoadBudgetPerFrame(per_frame);
    mgr->SetMaxConcurrentLoads(max_concurrent);
    return 0;
}

// streaming.get_active_loads() → int
int L_StreamingGetActiveLoads(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) { lua_pushinteger(L, 0); return 1; }

    lua_pushinteger(L, mgr->GetActiveLoadCount());
    return 1;
}

// streaming.get_zone_count() → int
int L_StreamingGetZoneCount(lua_State* L) {
    auto* mgr = GetStreamingManager();
    if (!mgr) { lua_pushinteger(L, 0); return 1; }

    lua_pushinteger(L, static_cast<lua_Integer>(mgr->GetZoneCount()));
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
