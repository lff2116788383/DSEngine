#include <gtest/gtest.h>
#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "engine/assets/asset_manager.h"
#include <utility>

using namespace dse::runtime;

class CppBusinessRuntimeTest : public ::testing::Test {
protected:
    void TearDown() override {
        ConfigureCppBusinessHooks({});
    }

    World world;
    AssetManager assets;
};

TEST_F(CppBusinessRuntimeTest, 未配置hooks时Bootstrap返回false) {
    ConfigureCppBusinessHooks({});
    EXPECT_FALSE(BootstrapCppBusiness(world, assets));
}

TEST_F(CppBusinessRuntimeTest, 缺少tick时Bootstrap返回false) {
    CppBusinessHooks hooks;
    hooks.bootstrap = [](World&, AssetManager&) {};
    ConfigureCppBusinessHooks(std::move(hooks));
    EXPECT_FALSE(BootstrapCppBusiness(world, assets));
}

TEST_F(CppBusinessRuntimeTest, BootstrapTickShutdown按配置调用) {
    int bootstrap_count = 0;
    int tick_count = 0;
    int shutdown_count = 0;
    float last_dt = 0.0f;

    CppBusinessHooks hooks;
    hooks.bootstrap = [&](World& w, AssetManager& a) {
        EXPECT_EQ(&w, &world);
        EXPECT_EQ(&a, &assets);
        ++bootstrap_count;
    };
    hooks.tick = [&](World& w, float dt) {
        EXPECT_EQ(&w, &world);
        last_dt = dt;
        ++tick_count;
    };
    hooks.shutdown = [&]() {
        ++shutdown_count;
    };
    ConfigureCppBusinessHooks(std::move(hooks));

    EXPECT_TRUE(BootstrapCppBusiness(world, assets));
    TickCppBusiness(world, 0.25f);
    ShutdownCppBusiness();

    EXPECT_EQ(bootstrap_count, 1);
    EXPECT_EQ(tick_count, 1);
    EXPECT_EQ(shutdown_count, 1);
    EXPECT_FLOAT_EQ(last_dt, 0.25f);
}

TEST_F(CppBusinessRuntimeTest, 未配置tick时Tick为NoOp) {
    CppBusinessHooks hooks;
    hooks.shutdown = []() {};
    ConfigureCppBusinessHooks(std::move(hooks));
    EXPECT_NO_THROW(TickCppBusiness(world, 0.016f));
}

TEST_F(CppBusinessRuntimeTest, 未配置shutdown时Shutdown为NoOp) {
    CppBusinessHooks hooks;
    hooks.tick = [](World&, float) {};
    ConfigureCppBusinessHooks(std::move(hooks));
    EXPECT_NO_THROW(ShutdownCppBusiness());
}
