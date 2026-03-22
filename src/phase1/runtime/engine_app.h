#ifndef DSE_PHASE1_ENGINE_APP_H
#define DSE_PHASE1_ENGINE_APP_H

#include <string>
#include "phase1/runtime/frame_pipeline.h"
class Phase1AssetManager;

namespace phase1::runtime {

struct EngineRunConfig {
    int window_width = 800;
    int window_height = 600;
    std::string window_title = "DSEngine Phase 1";
    Phase1BusinessMode business_mode = Phase1BusinessMode::Lua;
    std::string startup_lua_script_path;
    Phase1World* world = nullptr;
    Phase1AssetManager* asset_manager = nullptr;
};

int RunEngine(const EngineRunConfig& config);

}

#endif
