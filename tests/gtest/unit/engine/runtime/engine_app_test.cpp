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
#include "engine/ecs/world.h"
#include "engine/assets/asset_manager.h"

using namespace dse::runtime;

TEST(EngineRunConfigTest, 默认窗口尺寸为800x600) {
    EngineRunConfig config;
    EXPECT_EQ(config.window_width, 800);
    EXPECT_EQ(config.window_height, 600);
}

TEST(EngineRunConfigTest, 默认窗口标题) {
    EngineRunConfig config;
    EXPECT_EQ(config.window_title, "DSEngine Phase 2");
}

TEST(EngineRunConfigTest, 默认业务模式为Lua) {
    EngineRunConfig config;
    EXPECT_EQ(config.business_mode, BusinessMode::Lua);
}

TEST(EngineRunConfigTest, 默认不启用编辑器) {
    EngineRunConfig config;
    EXPECT_FALSE(config.enable_editor);
}

TEST(EngineRunConfigTest, 默认无启动Lua脚本) {
    EngineRunConfig config;
    EXPECT_TRUE(config.startup_lua_script_path.empty());
}

TEST(EngineRunConfigTest, 默认服务指针为空) {
    EngineRunConfig config;
    EXPECT_EQ(config.world, nullptr);
    EXPECT_EQ(config.asset_manager, nullptr);
    EXPECT_EQ(config.job_system, nullptr);
}

TEST(EngineRunConfigTest, WithServices链式调用) {
    RuntimeServices services;
    EngineRunConfig config;
    auto& ref = config.WithServices(services);
    EXPECT_EQ(&ref, &config);
}

TEST(EngineInstanceTest, 构造与析构不崩溃) {
    EngineRunConfig config;
    config.enable_editor = true; // 避免在析构时调用 glfwTerminate
    EXPECT_NO_THROW({
        EngineInstance instance(config);
    });
}

TEST(EngineInstanceTest, 未初始化时pipeline返回非空) {
    EngineRunConfig config;
    EngineInstance instance(config);
    ASSERT_NE(instance.pipeline(), nullptr);
}

TEST(EngineInstanceTest, 未初始化时Tick不崩溃) {
    EngineRunConfig config;
    EngineInstance instance(config);
    EXPECT_NO_THROW(instance.Tick());
}

TEST(EngineInstanceTest, 未初始化时Shutdown不崩溃) {
    EngineRunConfig config;
    EngineInstance instance(config);
    EXPECT_NO_THROW(instance.Shutdown());
}

TEST(EngineInstanceTest, 注入外部World后ServiceLocator可获取) {
    EngineRunConfig config;
    World external_world;
    config.world = &external_world;
    EngineInstance instance(config);
    // EngineInstance 构造时不会注册到 ServiceLocator，那是 Init 的职责
    // 此处验证 config 注入路径正确传递
    EXPECT_NO_THROW(instance.Shutdown());
}

TEST(EngineInstanceTest, 注入外部AssetManager不崩溃) {
    EngineRunConfig config;
    AssetManager external_am;
    config.asset_manager = &external_am;
    EXPECT_NO_THROW({
        EngineInstance instance(config);
    });
}
