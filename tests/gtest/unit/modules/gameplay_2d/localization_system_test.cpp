/**
* @file localization_system_test.cpp
* @brief 国际化系统单元测试，验证多语言文本查找、参数替换、RTL 检测、回调等核心功能
*/

#include <gtest/gtest.h>
#include "modules/gameplay_2d/localization/localization_system.h"

using namespace dse::gameplay2d;

TEST(LocalizationSystemTest, 默认语言为en) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetCurrentLanguage(), "en");
}

TEST(LocalizationSystemTest, 键不存在时返回默认文本) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetText("missing.key", "fallback"), "fallback");
}

TEST(LocalizationSystemTest, 键不存在且无默认返回空) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetText("missing.key"), "");
}

TEST(LocalizationSystemTest, 设置未加载的语言返回失败) {
    LocalizationSystem loc;
    EXPECT_FALSE(loc.SetCurrentLanguage("zh"));
}

TEST(LocalizationSystemTest, 默认无已加载语言) {
    LocalizationSystem loc;
    auto langs = loc.GetAvailableLanguages();
    EXPECT_TRUE(langs.empty());
}

TEST(LocalizationSystemTest, 检测英文为LTR方向) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.DetectTextDirection("Hello"), TextDirection::LTR);
}

TEST(LocalizationSystemTest, 阿拉伯语为RTL语言) {
    LocalizationSystem loc;
    EXPECT_TRUE(loc.IsRTLLanguage("ar"));
}

TEST(LocalizationSystemTest, 英语不是RTL语言) {
    LocalizationSystem loc;
    EXPECT_FALSE(loc.IsRTLLanguage("en"));
}

TEST(LocalizationSystemTest, 无参数时GetTextWithParams返回默认文本) {
    LocalizationSystem loc;
    std::unordered_map<std::string, std::string> params;
    EXPECT_EQ(loc.GetTextWithParams("missing.key", params, "Hello"), "Hello");
}

TEST(LocalizationSystemTest, 带参数替换获取文本) {
    LocalizationSystem loc;
    std::unordered_map<std::string, std::string> params = {{"name", "Player"}};
    EXPECT_EQ(loc.GetTextWithParams("missing.key", params, "Hello {name}"), "Hello Player");
}

TEST(LocalizationSystemTest, 语言变更回调注册与注销) {
    LocalizationSystem loc;
    std::string received_lang;
    int cb_id = loc.OnLanguageChanged([&](const std::string& lang) {
        received_lang = lang;
    });
    EXPECT_GE(cb_id, 0);
    loc.UnregisterLanguageChangeCallback(cb_id);
}

TEST(LocalizationSystemTest, Clear清空数据) {
    LocalizationSystem loc;
    loc.Clear();
    EXPECT_EQ(loc.GetCurrentLanguage(), "en");
    EXPECT_TRUE(loc.GetAvailableLanguages().empty());
}
