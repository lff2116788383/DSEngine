#ifndef DSE_PHASE1_LUA_RUNTIME_H
#define DSE_PHASE1_LUA_RUNTIME_H

#include <functional>
#include <string>
#include "phase1/ecs/world.h"
class Phase1AssetManager;

namespace phase1::runtime {

struct LuaApiContext {
    Phase1World* world = nullptr;
    std::function<void(const std::string&)> set_window_title;
    std::function<int()> get_draw_calls;
    std::function<int()> get_max_batch_sprites;
    std::function<int()> get_sprite_count;
    Phase1AssetManager* asset_manager = nullptr;
};

void ConfigureLuaApiContext(LuaApiContext context);
void SetStartupLuaScriptPath(std::string script_path);
bool BootstrapLuaRuntime();
void TickLuaRuntime(float delta_time);
void ShutdownLuaRuntime();

}

#endif
