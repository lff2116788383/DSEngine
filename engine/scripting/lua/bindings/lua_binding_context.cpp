/**
 * @file lua_binding_context.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/native_api/dse_api.h"
#include "engine/assets/asset_manager.h"
#include <stdexcept>

namespace dse::runtime::lua_binding {
namespace {
LuaApiContext g_binding_context{};
}

int L_GetMemoryUsage(lua_State* L) {
    size_t mem = runtime::GetLuaMemoryUsage();
    lua_pushnumber(L, static_cast<lua_Number>(mem) / 1024.0); // Return KB
    return 1;
}

void ConfigureBindingContext(const LuaApiContext& context) {
    g_binding_context = context;

    dse_native_api_init(
        context.world,
        context.asset_manager,
        context.audio_system,
        []() { if (g_binding_context.quit_app) g_binding_context.quit_app(); },
        [](const char* t) { if (g_binding_context.set_window_title) g_binding_context.set_window_title(t); },
        []() -> float { return g_binding_context.get_target_fps ? g_binding_context.get_target_fps() : 60.0f; },
        [](float f) { if (g_binding_context.set_target_fps) g_binding_context.set_target_fps(f); },
        []() -> int { return g_binding_context.get_draw_calls ? g_binding_context.get_draw_calls() : 0; });
}

void RegisterContextBindings(lua_State* L) {
    lua_getglobal(L, "dse");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
        lua_setglobal(L, "dse");
        lua_getglobal(L, "dse");
    }

    lua_pushcfunction(L, L_GetMemoryUsage); lua_setfield(L, -2, "get_memory_usage_kb");

    lua_pop(L, 1); // pop dse
}

const LuaApiContext& GetBindingContext() {
    return g_binding_context;
}

World* GetWorld() {
    return g_binding_context.world;
}

AssetManager& GetAssetManager() {
    if (g_binding_context.asset_manager) {
        return *g_binding_context.asset_manager;
    }
    throw std::runtime_error("Lua binding context requires an injected AssetManager");
}

Entity LuaEntityFromInteger(lua_Integer value) {
    return static_cast<Entity>(static_cast<std::uint32_t>(value));
}

}
