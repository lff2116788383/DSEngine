/**
 * @file runtime_context_test.cpp
 * @brief RuntimeContext 运行时上下文单元测试
 *
 * 覆盖场景：
 * - 默认构造字段为零值/空指针
 * - business_mode 默认为 Lua
 * - editor_mode 默认为 false
 * - 字段可修改
 */

#include <gtest/gtest.h>
#include "engine/runtime/runtime_context.h"
#include "engine/runtime/runtime_services.h"

TEST(RuntimeContextTest, DefaultisZero) {
    dse::runtime::RuntimeContext ctx;
    EXPECT_EQ(ctx.world, nullptr);
    EXPECT_EQ(ctx.asset_manager, nullptr);
    EXPECT_EQ(ctx.rhi_device, nullptr);
    EXPECT_FALSE(ctx.editor_mode);
    EXPECT_FALSE(static_cast<bool>(ctx.window_title_setter));
}

TEST(RuntimeContextTest, business_ModeDefaultIsLua) {
    dse::runtime::RuntimeContext ctx;
    EXPECT_EQ(ctx.business_mode, BusinessMode::Lua);
}

TEST(RuntimeContextTest, CanRevise) {
    dse::runtime::RuntimeContext ctx;
    ctx.business_mode = BusinessMode::Cpp;
    ctx.editor_mode = true;
    EXPECT_EQ(ctx.business_mode, BusinessMode::Cpp);
    EXPECT_TRUE(ctx.editor_mode);
}

TEST(RuntimeContextTest, window_title_SetterInjectable) {
    dse::runtime::RuntimeContext ctx;
    std::string captured;
    ctx.window_title_setter = [&captured](const std::string& title) {
        captured = title;
    };
    ctx.window_title_setter("hello");
    EXPECT_EQ(captured, "hello");
}

TEST(RuntimeServicesTest, DefaultWithIsEmpty) {
    dse::runtime::RuntimeServices services;
    EXPECT_EQ(services.world, nullptr);
    EXPECT_EQ(services.asset_manager, nullptr);
    EXPECT_EQ(services.job_system, nullptr);
}

TEST(BusinessModeTest, LuaAndCppenumerationValuesAreDifferent) {
    EXPECT_NE(BusinessMode::Lua, BusinessMode::Cpp);
}
