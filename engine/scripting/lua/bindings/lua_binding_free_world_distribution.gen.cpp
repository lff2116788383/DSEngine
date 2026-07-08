/**
 * @file lua_binding_free_world_distribution.gen.cpp
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

int L_dse_dist_init(lua_State* L) {
    float cell_size = 512.0f;
    int max_downloads = 4;
    const char* cdn_url = nullptr;
    if (lua_istable(L, 1)) {
        lua_getfield(L, 1, "cell_size");
        if (!lua_isnil(L, -1)) cell_size = static_cast<float>(lua_tonumber(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "max_downloads");
        if (!lua_isnil(L, -1)) max_downloads = static_cast<int>(lua_tointeger(L, -1));
        lua_pop(L, 1);
        lua_getfield(L, 1, "cdn_url");
        if (!lua_isnil(L, -1)) cdn_url = lua_tostring(L, -1);
        lua_pop(L, 1);
    }
    int _ret = dse_dist_init(cell_size, max_downloads, cdn_url);
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_dist_shutdown(lua_State* L) {
    dse_dist_shutdown();
    return 0;
}

int L_dse_dist_load_manifest(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_dist_load_manifest(path);
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_dist_save_manifest(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    int _ret = dse_dist_save_manifest(path);
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_dist_package_cell(lua_State* L) {
    int cx = static_cast<int>(luaL_checkinteger(L, 1));
    int cz = static_cast<int>(luaL_checkinteger(L, 2));
    int lod = static_cast<int>(luaL_optinteger(L, 3, 0));
    std::vector<std::string> assets_storage;
    std::vector<const char*> assets;
    if (lua_istable(L, 4)) {
        lua_Integer _n = static_cast<lua_Integer>(lua_rawlen(L, 4));
        for (lua_Integer _i = 1; _i <= _n; ++_i) {
            lua_rawgeti(L, 4, _i);
            if (lua_isstring(L, -1)) assets_storage.emplace_back(lua_tostring(L, -1));
            lua_pop(L, 1);
        }
        for (const auto& _s : assets_storage) assets.push_back(_s.c_str());
    }
    int _ret = dse_dist_package_cell(cx, cz, lod, assets.data(), static_cast<int>(assets.size()));
    lua_pushinteger(L, _ret);
    return 1;
}

int L_dse_dist_request_download(lua_State* L) {
    const char* package_id = luaL_checkstring(L, 1);
    dse_dist_request_download(package_id);
    return 0;
}

int L_dse_dist_cancel_download(lua_State* L) {
    const char* package_id = luaL_checkstring(L, 1);
    dse_dist_cancel_download(package_id);
    return 0;
}

int L_dse_dist_update_priorities(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    dse_dist_update_priorities(x, y, z);
    return 0;
}

int L_dse_dist_tick(lua_State* L) {
    float dt = static_cast<float>(luaL_optnumber(L, 1, 0.016));
    dse_dist_tick(dt);
    return 0;
}

int L_dse_dist_is_installed(lua_State* L) {
    const char* package_id = luaL_checkstring(L, 1);
    int _ret = dse_dist_is_installed(package_id);
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_dist_get_stats(lua_State* L) {
    int total = 0;
    int installed = 0;
    int downloading = 0;
    int pending = 0;
    double dl_bytes = 0;
    double speed = 0;
    dse_dist_get_stats(&total, &installed, &downloading, &pending, &dl_bytes, &speed);
    lua_newtable(L);
    lua_pushinteger(L, total);
    lua_setfield(L, -2, "total_packages");
    lua_pushinteger(L, installed);
    lua_setfield(L, -2, "installed");
    lua_pushinteger(L, downloading);
    lua_setfield(L, -2, "downloading");
    lua_pushinteger(L, pending);
    lua_setfield(L, -2, "pending");
    lua_pushnumber(L, dl_bytes);
    lua_setfield(L, -2, "downloaded_bytes");
    lua_pushnumber(L, speed);
    lua_setfield(L, -2, "speed_bps");
    return 1;
}

int L_dse_dist_get_missing(lua_State* L) {
    float x = static_cast<float>(luaL_checknumber(L, 1));
    float y = static_cast<float>(luaL_checknumber(L, 2));
    float z = static_cast<float>(luaL_checknumber(L, 3));
    float radius = static_cast<float>(luaL_optnumber(L, 4, 512.0));
    char _buf[8192];
    int _count = dse_dist_get_missing(x, y, z, radius, _buf, sizeof(_buf));
    lua_newtable(L);
    int _offset = 0;
    int _idx = 1;
    for (int _i = 0; _i < _count && _offset < static_cast<int>(sizeof(_buf)); ++_i) {
        const char* _name = _buf + _offset;
        lua_pushstring(L, _name);
        lua_rawseti(L, -2, _idx++);
        _offset += static_cast<int>(strlen(_name)) + 1;
    }
    return 1;
}

int L_dse_dist_verify(lua_State* L) {
    const char* package_id = luaL_checkstring(L, 1);
    int _ret = dse_dist_verify(package_id);
    lua_pushboolean(L, _ret);
    return 1;
}

int L_dse_dist_get_disk_usage(lua_State* L) {
    uint32_t _ret = dse_dist_get_disk_usage();
    lua_pushinteger(L, static_cast<lua_Integer>(_ret));
    return 1;
}

} // namespace

void RegisterFreeWorldDistBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "distribution");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_pushvalue(L, -1);
        lua_setfield(L, -3, "distribution");
    }
    helper::RegisterBindings(L, {
        {"init", L_dse_dist_init},
        {"shutdown", L_dse_dist_shutdown},
        {"load_manifest", L_dse_dist_load_manifest},
        {"save_manifest", L_dse_dist_save_manifest},
        {"package_cell", L_dse_dist_package_cell},
        {"request_download", L_dse_dist_request_download},
        {"cancel_download", L_dse_dist_cancel_download},
        {"update_priorities", L_dse_dist_update_priorities},
        {"tick", L_dse_dist_tick},
        {"is_installed", L_dse_dist_is_installed},
        {"get_stats", L_dse_dist_get_stats},
        {"get_missing", L_dse_dist_get_missing},
        {"verify", L_dse_dist_verify},
        {"get_disk_usage", L_dse_dist_get_disk_usage},
    });
    lua_pop(L, 2);
}

} // namespace dse::runtime::lua_binding
