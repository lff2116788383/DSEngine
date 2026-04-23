/**
 * @file engine_app.h
 * @brief 引擎应用外壳，负责运行时生命周期与服务装配。
 */

#ifndef DSE_ENGINE_APP_H
#define DSE_ENGINE_APP_H

#include <string>
#include <memory>
#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/runtime_services.h"
#include "engine/assets/asset_manager.h"

namespace dse::runtime {

struct EngineRunConfig {
    int window_width = 800;
    int window_height = 600;
    std::string window_title = "DSEngine Phase 2";
    BusinessMode business_mode = BusinessMode::Lua;
    bool enable_editor = false;
    std::string startup_lua_script_path;
    RuntimeServices services{};
    // Backward-compatible aliases; EngineInstance will fold them into services.
    World* world = nullptr;
    AssetManager* asset_manager = nullptr;

    EngineRunConfig& WithServices(RuntimeServices runtime_services) {
        services = runtime_services;
        return *this;
    }
};

/**
 * @class EngineInstance
 * @brief 引擎应用运行实例，负责生命周期、默认服务装配与主循环驱动。
 */
class EngineInstance {
public:
    EngineInstance(const EngineRunConfig& config);
    ~EngineInstance();

    /**
     * @brief 初始化引擎（不包含自带的窗口循环）
     * @return 成功返回 true
     */
    bool Init();

    /**
     * @brief 执行引擎的单帧更新
     */
    void Tick();

    /**
     * @brief 清理引擎资源
     */
    void Shutdown();

    /**
     * @brief 执行完整的引擎生命周期（包含默认主循环）
     * @return 退出码
     */
    int Run();

    /**
     * @brief 获取渲染管线
     */
    FramePipeline* pipeline() const { return pipeline_.get(); }

private:
    /**
     * @brief 运行启动期场景回归样例。
     * @return 所有样例通过返回 true
     */
    bool RunStartupSceneRegressionChecks();

    /**
     * @brief Init() 失败路径的统一清理，避免多处重复清理代码。
     */
    void CleanupOnInitFailure();

    EngineRunConfig config_;
    RuntimeServices services_{};
    std::unique_ptr<World> default_world_;
    std::unique_ptr<AssetManager> default_asset_manager_;
    std::unique_ptr<FramePipeline> pipeline_;
    float accumulator_ = 0.0f;
    float fixed_time_step_ = 0.02f;
    bool is_initialized_ = false;
};

int RunEngine(const EngineRunConfig& config); // Keep for backwards compatibility

}

#endif

