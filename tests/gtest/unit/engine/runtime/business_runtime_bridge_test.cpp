/**
 * @file business_runtime_bridge_test.cpp
 * @brief BusinessRuntimeBridge 单元测试 — Cpp 模式的 Bootstrap/Tick/Shutdown 路径
 */

#include <gtest/gtest.h>
#include "engine/runtime/business_runtime_bridge.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "engine/ecs/world.h"
#include "engine/assets/asset_manager.h"

using namespace dse::runtime;

class BusinessRuntimeBridgeTest : public ::testing::Test {
protected:
    void SetUp() override {
        world_ = std::make_unique<World>();
        asset_mgr_ = std::make_unique<AssetManager>();

        ctx_.world = world_.get();
        ctx_.asset_manager = asset_mgr_.get();
        ctx_.business_mode = BusinessMode::Cpp;

        bootstrap_called_ = false;
        tick_count_ = 0;
        shutdown_called_ = false;
    }

    void TearDown() override {
        ShutdownBusinessRuntime(ctx_);
        ConfigureCppBusinessHooks({});
        world_.reset();
        asset_mgr_.reset();
    }

    RuntimeContext ctx_;
    std::unique_ptr<World> world_;
    std::unique_ptr<AssetManager> asset_mgr_;

    static bool bootstrap_called_;
    static int tick_count_;
    static bool shutdown_called_;
};

bool BusinessRuntimeBridgeTest::bootstrap_called_ = false;
int BusinessRuntimeBridgeTest::tick_count_ = 0;
bool BusinessRuntimeBridgeTest::shutdown_called_ = false;

TEST_F(BusinessRuntimeBridgeTest, CppModeBootstrapWithHooks) {
    ConfigureCppBusinessHooks({
        [](World&, AssetManager&) { bootstrap_called_ = true; },
        [](World&, float) { tick_count_++; },
        []() { shutdown_called_ = true; }
    });

    RuntimeStatsBindings stats{
        []() -> int { return 0; },
        []() -> int { return 0; },
        []() -> int { return 0; }
    };

    EXPECT_TRUE(BootstrapBusinessRuntime(ctx_, stats));
    EXPECT_TRUE(bootstrap_called_);
}

TEST_F(BusinessRuntimeBridgeTest, CppModeTick) {
    ConfigureCppBusinessHooks({
        [](World&, AssetManager&) {},
        [](World&, float) { tick_count_++; },
        []() {}
    });

    RuntimeStatsBindings stats{
        []() -> int { return 0; },
        []() -> int { return 0; },
        []() -> int { return 0; }
    };

    BootstrapBusinessRuntime(ctx_, stats);

    TickBusinessRuntime(ctx_, 0.016f);
    TickBusinessRuntime(ctx_, 0.016f);
    EXPECT_EQ(tick_count_, 2);
}

TEST_F(BusinessRuntimeBridgeTest, CppModeShutdown) {
    ConfigureCppBusinessHooks({
        [](World&, AssetManager&) {},
        [](World&, float) {},
        []() { shutdown_called_ = true; }
    });

    RuntimeStatsBindings stats{
        []() -> int { return 0; },
        []() -> int { return 0; },
        []() -> int { return 0; }
    };

    BootstrapBusinessRuntime(ctx_, stats);
    ShutdownBusinessRuntime(ctx_);
    EXPECT_TRUE(shutdown_called_);
}

TEST_F(BusinessRuntimeBridgeTest, CppModeNullWorldFails) {
    RuntimeContext ctx;
    ctx.business_mode = BusinessMode::Cpp;
    ctx.world = nullptr;
    ctx.asset_manager = asset_mgr_.get();

    RuntimeStatsBindings stats{
        []() -> int { return 0; },
        []() -> int { return 0; },
        []() -> int { return 0; }
    };

    EXPECT_FALSE(BootstrapBusinessRuntime(ctx, stats));
}

TEST_F(BusinessRuntimeBridgeTest, CppModeNullAssetManagerFails) {
    RuntimeContext ctx;
    ctx.business_mode = BusinessMode::Cpp;
    ctx.world = world_.get();
    ctx.asset_manager = nullptr;

    RuntimeStatsBindings stats{
        []() -> int { return 0; },
        []() -> int { return 0; },
        []() -> int { return 0; }
    };

    EXPECT_FALSE(BootstrapBusinessRuntime(ctx, stats));
}

TEST_F(BusinessRuntimeBridgeTest, TickWithNullWorldSafe) {
    RuntimeContext ctx;
    ctx.business_mode = BusinessMode::Cpp;
    ctx.world = nullptr;

    TickBusinessRuntime(ctx, 0.016f);
}
