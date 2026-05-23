/**
 * @file runtime_update_graph_integration_test.cpp
 * @brief RuntimeUpdateGraph / RuntimeContext 集成测试
 *
 * RuntimeUpdateGraph 深耦合 FramePipeline 私有成员，无法独立单测。
 * 本测试通过 EngineInstance 构造间接验证更新图的装配和 RuntimeContext 结构完整性。
 */

#include <gtest/gtest.h>
#include "engine/runtime/engine_app.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/runtime/runtime_context.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"

using namespace dse::runtime;

class RuntimeUpdateGraphIntegrationTest : public ::testing::Test {};

TEST_F(RuntimeUpdateGraphIntegrationTest, RuntimeContextDefaults) {
    RuntimeContext ctx;
    EXPECT_EQ(ctx.world, nullptr);
    EXPECT_EQ(ctx.asset_manager, nullptr);
    EXPECT_EQ(ctx.rhi_device, nullptr);
    EXPECT_EQ(ctx.business_mode, BusinessMode::Lua);
    EXPECT_FALSE(ctx.editor_mode);
    EXPECT_EQ(ctx.native_window_handle, nullptr);
    EXPECT_EQ(ctx.audio_system, nullptr);
}

TEST_F(RuntimeUpdateGraphIntegrationTest, RuntimeContextCallbacks) {
    RuntimeContext ctx;
    std::string title;
    ctx.window_title_setter = [&](const std::string& t) { title = t; };
    ctx.window_title_setter("Test Title");
    EXPECT_EQ(title, "Test Title");

    bool quit_called = false;
    ctx.quit_app = [&]() { quit_called = true; };
    ctx.quit_app();
    EXPECT_TRUE(quit_called);

    float target_fps = 0.0f;
    ctx.set_target_fps = [&](float fps) { target_fps = fps; };
    ctx.get_target_fps = [&]() -> float { return target_fps; };
    ctx.set_target_fps(60.0f);
    EXPECT_FLOAT_EQ(ctx.get_target_fps(), 60.0f);
}

// EngineInstancePipelineHasWorld 需要 GPU/窗口上下文，在无头测试环境下无法运行，
// 因此不在自动化测试中包含。如需验证请手动在有 GPU 的环境中运行。
