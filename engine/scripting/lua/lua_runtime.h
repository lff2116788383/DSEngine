#ifndef DSE_LUA_RUNTIME_H
#define DSE_LUA_RUNTIME_H

#include <functional>
#include <string>
#include "engine/ecs/world.h"
class AssetManager;

namespace dse::runtime {

struct LuaApiContext {
    World* world = nullptr;
    std::function<void(const std::string&)> set_window_title;
    std::function<int()> get_draw_calls;
    std::function<int()> get_max_batch_sprites;
    std::function<int()> get_sprite_count;
    AssetManager* asset_manager = nullptr;
};

void ConfigureLuaApiContext(LuaApiContext context);
void SetStartupLuaScriptPath(std::string script_path);
bool BootstrapLuaRuntime();
void TickLuaRuntime(float delta_time);
void ShutdownLuaRuntime();

}

#endif
