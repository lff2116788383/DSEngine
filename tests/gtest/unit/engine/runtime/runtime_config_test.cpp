/**
 * @file runtime_config_test.cpp
 * @brief EngineRunConfig / RuntimeContext 默认值与配置链测试
 */

#include <gtest/gtest.h>
#include "engine/runtime/engine_app.h"
#include "engine/runtime/runtime_context.h"
#include "engine/core/job_system.h"

using namespace dse::runtime;

// ============================================================
// EngineRunConfig
// ============================================================

TEST(EngineRunConfigTest, DefaultValues) {
    EngineRunConfig cfg;
    EXPECT_EQ(cfg.window_width, 800);
    EXPECT_EQ(cfg.window_height, 600);
    EXPECT_EQ(cfg.window_title, "DSEngine Phase 2");
    EXPECT_EQ(cfg.business_mode, BusinessMode::Lua);
    EXPECT_FALSE(cfg.enable_editor);
    EXPECT_TRUE(cfg.startup_lua_script_path.empty());
}

TEST(EngineRunConfigTest, WithServicesChain) {
    RuntimeServices svc;
    svc.job_system = reinterpret_cast<dse::core::JobSystem*>(0x1234);

    EngineRunConfig cfg;
    auto& ref = cfg.WithServices(svc);

    EXPECT_EQ(&ref, &cfg);
    EXPECT_EQ(cfg.services.job_system, svc.job_system);
}

TEST(EngineRunConfigTest, CustomValues) {
    EngineRunConfig cfg;
    cfg.window_width = 1920;
    cfg.window_height = 1080;
    cfg.window_title = "CustomTitle";
    cfg.business_mode = BusinessMode::Cpp;
    cfg.enable_editor = true;
    cfg.startup_lua_script_path = "scripts/main.lua";

    EXPECT_EQ(cfg.window_width, 1920);
    EXPECT_EQ(cfg.window_height, 1080);
    EXPECT_EQ(cfg.window_title, "CustomTitle");
    EXPECT_EQ(cfg.business_mode, BusinessMode::Cpp);
    EXPECT_TRUE(cfg.enable_editor);
    EXPECT_EQ(cfg.startup_lua_script_path, "scripts/main.lua");
}

// ============================================================
// RuntimeContext
// ============================================================

TEST(RuntimeContextTest, DefaultNullPointers) {
    RuntimeContext ctx;
    EXPECT_EQ(ctx.world, nullptr);
    EXPECT_EQ(ctx.asset_manager, nullptr);
    EXPECT_EQ(ctx.rhi_device, nullptr);
    EXPECT_EQ(ctx.native_window_handle, nullptr);
    EXPECT_EQ(ctx.audio_system, nullptr);
}

TEST(RuntimeContextTest, DefaultBusinessModeLua) {
    RuntimeContext ctx;
    EXPECT_EQ(ctx.business_mode, BusinessMode::Lua);
    EXPECT_FALSE(ctx.editor_mode);
}

TEST(RuntimeContextTest, CallbacksDefaultNull) {
    RuntimeContext ctx;
    EXPECT_FALSE(static_cast<bool>(ctx.window_title_setter));
    EXPECT_FALSE(static_cast<bool>(ctx.quit_app));
    EXPECT_FALSE(static_cast<bool>(ctx.set_target_fps));
    EXPECT_FALSE(static_cast<bool>(ctx.get_target_fps));
    EXPECT_FALSE(static_cast<bool>(ctx.make_render_context_current));
    EXPECT_FALSE(static_cast<bool>(ctx.release_render_context));
    EXPECT_FALSE(static_cast<bool>(ctx.present_frame));
}

TEST(RuntimeContextTest, InjectAndInvokeCallbacks) {
    RuntimeContext ctx;

    std::string title;
    ctx.window_title_setter = [&](const std::string& t) { title = t; };
    ctx.window_title_setter("Hello");
    EXPECT_EQ(title, "Hello");

    bool quit = false;
    ctx.quit_app = [&]() { quit = true; };
    ctx.quit_app();
    EXPECT_TRUE(quit);

    float fps = 0.0f;
    ctx.set_target_fps = [&](float f) { fps = f; };
    ctx.get_target_fps = [&]() -> float { return fps; };
    ctx.set_target_fps(144.0f);
    EXPECT_FLOAT_EQ(ctx.get_target_fps(), 144.0f);
}

// ============================================================
// BusinessMode enum
// ============================================================

TEST(BusinessModeTest, EnumValues) {
    EXPECT_EQ(static_cast<int>(BusinessMode::Lua), 0);
    EXPECT_EQ(static_cast<int>(BusinessMode::Cpp), 1);
}
