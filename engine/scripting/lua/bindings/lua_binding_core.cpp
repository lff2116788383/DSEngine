#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
extern "C" {
#include <lauxlib.h>
}

namespace dse::runtime::lua_binding {
namespace {
int L_AssetsLoadTexture(lua_State* L) {
    const char* path = luaL_checkstring(L, 1);
    auto texture = GetAssetManager().LoadTexture(path);
    lua_pushinteger(L, texture ? static_cast<lua_Integer>(texture->GetHandle()) : 0);
    return 1;
}

int L_AppSetDataRoot(lua_State* L) {
    const char* data_root = luaL_checkstring(L, 1);
    auto& asset_manager = GetAssetManager();
    asset_manager.ConfigureDataRoot(data_root);
    DEBUG_LOG_INFO("Data root updated from lua: {}", asset_manager.GetDataRoot());
    return 0;
}

int L_AppSetWindowTitle(lua_State* L) {
    const char* title = luaL_checkstring(L, 1);
    const auto& setter = GetBindingContext().set_window_title;
    if (setter) {
        setter(title);
    }
    return 0;
}

int L_MetricsGetDrawCalls(lua_State* L) {
    const auto& fn = GetBindingContext().get_draw_calls;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

int L_MetricsGetMaxBatchSprites(lua_State* L) {
    const auto& fn = GetBindingContext().get_max_batch_sprites;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}

int L_MetricsGetSpriteCount(lua_State* L) {
    const auto& fn = GetBindingContext().get_sprite_count;
    lua_pushinteger(L, fn ? fn() : 0);
    return 1;
}
}

void RegisterAssetsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("load_texture", L_AssetsLoadTexture);
}

void RegisterAppBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("set_data_root", L_AppSetDataRoot);
    set_fn("set_window_title", L_AppSetWindowTitle);
}

void RegisterMetricsBindings(lua_State* L) {
    auto set_fn = [L](const char* name, lua_CFunction fn) {
        lua_pushcfunction(L, fn);
        lua_setfield(L, -2, name);
    };
    lua_newtable(L);
    set_fn("get_draw_calls", L_MetricsGetDrawCalls);
    set_fn("get_max_batch_sprites", L_MetricsGetMaxBatchSprites);
    set_fn("get_sprite_count", L_MetricsGetSpriteCount);
}

}
