/**
 * @file engine_app.h
 * @brief 引擎核心模块，提供基础功能支持
 */

#ifndef DSE_ENGINE_APP_H
#define DSE_ENGINE_APP_H

#include <string>
#include <memory>
#include "engine/runtime/frame_pipeline.h"
#include "engine/assets/asset_manager.h"

namespace dse::runtime {

struct EngineRunConfig {
    int window_width = 800;
    int window_height = 600;
    std::string window_title = "DSEngine Phase 2";
    BusinessMode business_mode = BusinessMode::Lua;
    bool enable_editor = false;
    std::string startup_lua_script_path;
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;
};

/**
 * @class EngineInstance
 * @brief 引擎独立运行实例，实现多实例解耦的基础
 */
class EngineInstance {
public:
    EngineInstance(const EngineRunConfig& config);
    ~EngineInstance();

    int Run();

private:
    EngineRunConfig config_;
    std::unique_ptr<World> default_world_;
    std::unique_ptr<AssetManager> default_asset_manager_;
    std::unique_ptr<FramePipeline> pipeline_;
};

int RunEngine(const EngineRunConfig& config); // Keep for backwards compatibility

}

#endif
