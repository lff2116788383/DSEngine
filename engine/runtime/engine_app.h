#ifndef DSE_ENGINE_APP_H
#define DSE_ENGINE_APP_H

#include <string>
#include "engine/runtime/frame_pipeline.h"
class AssetManager;

namespace dse::runtime {

struct EngineRunConfig {
    int window_width = 800;
    int window_height = 600;
    std::string window_title = "DSEngine Phase 1";
    BusinessMode business_mode = BusinessMode::Lua;
    std::string startup_lua_script_path;
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
};

int RunEngine(const EngineRunConfig& config);

}

#endif
