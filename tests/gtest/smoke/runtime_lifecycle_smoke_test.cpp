/**
 * @file runtime_lifecycle_smoke_test.cpp
 * @brief 运行时生命周期冒烟测试
 *
 * 验证场景：
 * - EngineRunConfig 默认值合理性
 * - RuntimeContext 各模式切换
 * - BusinessRuntimeBridge Lua/Cpp 模式分派
 * - RuntimeServices 默认与注入
 * - FramePipeline 未初始化安全操作
 * - 核心服务（EventBus+JobSystem+World）快速启停
 *
 * 注意：EngineInstance::Init 需要 GLFW，不在本测试中覆盖。
 */

#include <gtest/gtest.h>
#include "engine/runtime/engine_app.h"
#include "engine/runtime/runtime_context.h"
#include "engine/runtime/runtime_services.h"
#include "engine/runtime/business_runtime_bridge.h"
#include "engine/runtime/frame_pipeline.h"
#include "engine/ecs/world.h"
#include "engine/core/event_bus.h"
#include "engine/core/service_locator.h"
#include "engine/core/job_system.h"

using namespace dse::runtime;
using namespace dse::core;

class RuntimeLifecycleSmokeTest : public ::testing::Test {
protected:
    void TearDown() override {
        ServiceLocator::Instance().Reset<EventBus>();
        ServiceLocator::Instance().Reset<JobSystem>();
    }
};

TEST_F(RuntimeLifecycleSmokeTest, EngineRunConfigDefaultValues) {
    EngineRunConfig config;
    EXPECT_EQ(config.window_width, 800);
    EXPECT_EQ(config.window_height, 600);
    EXPECT_EQ(config.business_mode, BusinessMode::Lua);
    EXPECT_FALSE(config.enable_editor);
}

TEST_F(RuntimeLifecycleSmokeTest, RuntimeContextModeSwitch) {
    RuntimeContext ctx;
    EXPECT_EQ(ctx.business_mode, BusinessMode::Lua);

    ctx.business_mode = BusinessMode::Cpp;
    EXPECT_EQ(ctx.business_mode, BusinessMode::Cpp);
}

TEST_F(RuntimeLifecycleSmokeTest, BusinessRuntimeBridgeWithoutWorldWhenCppModeBootFailed) {
    RuntimeContext ctx;
    ctx.business_mode = BusinessMode::Cpp;
    ctx.world = nullptr;
    ctx.asset_manager = nullptr;

    RuntimeStatsBindings stats;
    bool result = BootstrapBusinessRuntime(ctx, stats);
    EXPECT_FALSE(result);
}

TEST_F(RuntimeLifecycleSmokeTest, FramePipelineSecurityOperationNotInitialized) {
    FramePipeline pipeline;
    EXPECT_NO_THROW(pipeline.Update(0.016f));
    EXPECT_NO_THROW(pipeline.FixedUpdate(0.02f));
    EXPECT_NO_THROW(pipeline.Render());
    EXPECT_NO_THROW(pipeline.Shutdown());
    EXPECT_EQ(pipeline.LastDrawCalls(), 0);
}

TEST_F(RuntimeLifecycleSmokeTest, TestCase5) {
    auto bus = std::make_shared<dse::core::EventBus>();
    ServiceLocator::Instance().Register<dse::core::EventBus, dse::core::EventBus>(bus);

    auto job_system = std::make_shared<dse::core::JobSystem>();
    job_system->Init();
    ServiceLocator::Instance().Register<dse::core::JobSystem, dse::core::JobSystem>(job_system);

    World world;

    // 快速使用
    auto e = world.CreateEntity();
    world.registry().emplace<TransformComponent>(e);
    EXPECT_TRUE(world.IsAlive(e));

    std::atomic<int> counter{0};
    auto h = job_system->Submit([&counter]() { counter.fetch_add(1); }, dse::core::JobPriority::Normal);
    job_system->Wait(h);
    EXPECT_EQ(counter.load(), 1);

    // 关闭
    world.DestroyEntity(e);
    job_system->Shutdown();
    ServiceLocator::Instance().Reset<dse::core::JobSystem>();
    ServiceLocator::Instance().Reset<dse::core::EventBus>();
}

TEST_F(RuntimeLifecycleSmokeTest, RuntimeServicesInjectionAndDefault) {
    RuntimeServices services;
    EXPECT_EQ(services.world, nullptr);
    EXPECT_EQ(services.asset_manager, nullptr);
    EXPECT_EQ(services.job_system, nullptr);

    World w;
    services.world = &w;
    EXPECT_EQ(services.world, &w);
}

TEST_F(RuntimeLifecycleSmokeTest, MultiTimesFramePipelineDoesNotCrash) {
    for (int i = 0; i < 3; ++i) {
        FramePipeline pipeline;
        pipeline.SetWorld(nullptr); // 未初始化
        EXPECT_NO_THROW(pipeline.Shutdown());
    }
}

TEST_F(RuntimeLifecycleSmokeTest, BusinessRuntimeBridgeLuaModeScriptlessBootFails) {
    RuntimeContext ctx;
    ctx.business_mode = BusinessMode::Lua;
    ctx.world = nullptr;

    RuntimeStatsBindings stats;
    bool result = BootstrapBusinessRuntime(ctx, stats);
    EXPECT_FALSE(result);
}
