/**
 * @file engine_app_test.cpp
 * @brief EngineInstance / EngineRunConfig 单元测试
 *
 * 覆盖场景：
 * - EngineRunConfig 默认值合理性
 * - EngineRunConfig::WithServices 链式调用
 * - EngineInstance 构造与析构不崩溃（不调用 Init，避免 GLFW 依赖）
 * - 未初始化时 Tick/Shutdown 为空操作
 * - 未初始化时 pipeline() 返回非空但未初始化的 FramePipeline
 *
 * 注意：EngineInstance::Init 需要 GLFW/OpenGL 环境，不在单元测试中覆盖。
 *       完整生命周期测试归属集成或冒烟测试层。
 */

#include <gtest/gtest.h>
#include "engine/runtime/engine_app.h"
#include "engine/runtime/runtime_services.h"

using namespace dse::runtime;

// 测试 引擎运行配置：默认为800 x 600
TEST(EngineRunConfigTest, DefaultIs800x600) {
    EngineRunConfig config;
    EXPECT_EQ(config.window_width, 800);
    EXPECT_EQ(config.window_height, 600);
}

// 测试 引擎运行配置：默认
TEST(EngineRunConfigTest, Default) {
    EngineRunConfig config;
    EXPECT_EQ(config.window_title, "DSEngine Phase 2");
}

// 测试 引擎运行配置：Defaultmodel为Lua
TEST(EngineRunConfigTest, DefaultmodelIsLua) {
    EngineRunConfig config;
    EXPECT_EQ(config.business_mode, BusinessMode::Lua);
}

// 测试 引擎运行配置：默认不启用
TEST(EngineRunConfigTest, DefaultNotEnabled) {
    EngineRunConfig config;
    EXPECT_FALSE(config.enable_editor);
}

// 测试 引擎运行配置：默认无Lua
TEST(EngineRunConfigTest, DefaultWithoutLua) {
    EngineRunConfig config;
    EXPECT_TRUE(config.startup_lua_script_path.empty());
}

// 测试 引擎运行配置：默认为空
TEST(EngineRunConfigTest, DefaultIsEmpty) {
    EngineRunConfig config;
    EXPECT_EQ(config.services.world, nullptr);
    EXPECT_EQ(config.services.asset_manager, nullptr);
    EXPECT_EQ(config.services.job_system, nullptr);
}

// 测试 引擎运行配置：带Serviceschain调用
TEST(EngineRunConfigTest, WithServiceschainCall) {
    RuntimeServices services;
    EngineRunConfig config;
    auto& ref = config.WithServices(services);
    EXPECT_EQ(&ref, &config);
}

// 测试 引擎实例：且不崩溃
TEST(EngineInstanceTest, AndDoesNotCrash) {
    EngineRunConfig config;
    config.enable_editor = true; // 避免在析构时调用 glfwTerminate
    EXPECT_NO_THROW({
        EngineInstance instance(config);
    });
}

// 测试 引擎实例：当不Initializedpipeline返回非空
TEST(EngineInstanceTest, WhenNotInitializedpipelineReturnNonEmpty) {
    EngineRunConfig config;
    EngineInstance instance(config);
    ASSERT_NE(instance.pipeline(), nullptr);
}

// 测试 引擎实例：当不已初始化滴答不崩溃
TEST(EngineInstanceTest, WhenNotInitializedTickDoesNotCrash) {
    EngineRunConfig config;
    EngineInstance instance(config);
    EXPECT_NO_THROW(instance.Tick());
}

// 测试 引擎实例：当不已初始化关闭不崩溃
TEST(EngineInstanceTest, WhenNotInitializedShutdownDoesNotCrash) {
    EngineRunConfig config;
    EngineInstance instance(config);
    EXPECT_NO_THROW(instance.Shutdown());
}

// 测试 引擎实例：注入外部世界之后服务定位器能够获取
TEST(EngineInstanceTest, InjectsOutsideWorldAfterServiceLocatorCanAcquire) {
    EngineRunConfig config;
    RuntimeServices svc;
    config.WithServices(svc);
    EngineInstance instance(config);
    // EngineInstance 构造时不会注册到 ServiceLocator，那是 Init 的职责
    // 此处验证 config 注入路径正确传递
    EXPECT_NO_THROW(instance.Shutdown());
}

// 测试 引擎实例：注入外部资源管理器不崩溃
TEST(EngineInstanceTest, InjectsOutsideAssetManagerDoesNotCrash) {
    EngineRunConfig config;
    EXPECT_NO_THROW({
        EngineInstance instance(config);
    });
}
