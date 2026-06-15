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

// 测试 C++业务运行时：不Configurationhooks当引导返回false
TEST_F(CppBusinessRuntimeTest, NotConfigurationhooksWhenBootstrapReturnsfalse) {
    ConfigureCppBusinessHooks({});
    EXPECT_FALSE(BootstrapCppBusiness(world, assets));
}

// 测试 C++业务运行时：Missingtick当引导返回false
TEST_F(CppBusinessRuntimeTest, MissingtickWhenBootstrapReturnsfalse) {
    CppBusinessHooks hooks;
    hooks.bootstrap = [](World&, AssetManager&) {};
    ConfigureCppBusinessHooks(std::move(hooks));
    EXPECT_FALSE(BootstrapCppBusiness(world, assets));
}

// 测试 C++业务运行时：引导滴答关闭调用按配置
TEST_F(CppBusinessRuntimeTest, BootstrapTickShutdownCallByConfiguration) {
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

// 测试 C++业务运行时：不Configurationtick当滴答为无操作
TEST_F(CppBusinessRuntimeTest, NotConfigurationtickWhenTickIsNoOp) {
    CppBusinessHooks hooks;
    hooks.shutdown = []() {};
    ConfigureCppBusinessHooks(std::move(hooks));
    EXPECT_NO_THROW(TickCppBusiness(world, 0.016f));
}

// 测试 C++业务运行时：不Configurationshutdown当关闭为无操作
TEST_F(CppBusinessRuntimeTest, NotConfigurationshutdownWhenShutdownIsNoOp) {
    CppBusinessHooks hooks;
    hooks.tick = [](World&, float) {};
    ConfigureCppBusinessHooks(std::move(hooks));
    EXPECT_NO_THROW(ShutdownCppBusiness());
}
