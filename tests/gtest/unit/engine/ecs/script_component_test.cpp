/**
 * @file script_component_test.cpp
 * @brief ScriptComponent / LuaScriptComponent 脚本组件的单元测试
 *
 * 覆盖场景：
 * - ScriptComponent 默认值与字段修改
 * - LuaScriptComponent 默认值
 */

#include <gtest/gtest.h>
#include "engine/ecs/script.h"

// 测试 脚本组件：默认值
TEST(ScriptComponentTest, DefaultValues) {
    ScriptComponent script;
    EXPECT_TRUE(script.script_path.empty());
    EXPECT_TRUE(script.enabled);
}

// 测试 脚本组件：设置上
TEST(ScriptComponentTest, SetUp) {
    ScriptComponent script;
    script.script_path = "scripts/player_controller.lua";
    script.enabled = false;
    EXPECT_EQ(script.script_path, "scripts/player_controller.lua");
    EXPECT_FALSE(script.enabled);
}

// 测试 Lua脚本组件：默认值
TEST(LuaScriptComponentTest, DefaultValues) {
    LuaScriptComponent lua;
    EXPECT_TRUE(lua.script_path.empty());
    EXPECT_FALSE(lua.is_initialized);
    EXPECT_EQ(lua.script_instance, nullptr);
}
