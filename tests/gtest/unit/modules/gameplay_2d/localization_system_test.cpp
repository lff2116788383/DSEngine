/**
* @file localization_system_test.cpp
* @brief 国际化系统单元测试，验证多语言文本查找、参数替换、RTL 检测、回调等核心功能
*/

#include <gtest/gtest.h>
#include "modules/gameplay_2d/localization/localization_system.h"

using namespace dse::gameplay2d;

// 测试 本地化系统：默认Isen
TEST(LocalizationSystemTest, DefaultIsen) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetCurrentLanguage(), "en");
}

// 测试 本地化系统：不存在当Returnsdefault
TEST(LocalizationSystemTest, DoesNotExistWhenReturnsdefault) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetText("missing.key", "fallback"), "fallback");
}

// 测试 本地化系统：不存在Withoutdefault返回空
TEST(LocalizationSystemTest, DoesNotExistWithoutdefaultReturnsEmpty) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetText("missing.key"), "");
}

// 测试 本地化系统：设置上不加载返回失败
TEST(LocalizationSystemTest, SetUpNotLoadReturnsFails) {
    LocalizationSystem loc;
    EXPECT_FALSE(loc.SetCurrentLanguage("zh"));
}

// 测试 本地化系统：默认无已经加载
TEST(LocalizationSystemTest, DefaultWithoutAlreadyLoad) {
    LocalizationSystem loc;
    auto langs = loc.GetAvailableLanguages();
    EXPECT_TRUE(langs.empty());
}

// 测试 本地化系统：为LTR朝向
TEST(LocalizationSystemTest, IsLTRToward) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.DetectTextDirection("Hello"), TextDirection::LTR);
}

// 测试 本地化系统：为RTL
TEST(LocalizationSystemTest, IsRTL) {
    LocalizationSystem loc;
    EXPECT_TRUE(loc.IsRTLLanguage("ar"));
}

// 测试 本地化系统：不RTL
TEST(LocalizationSystemTest, NotRTL) {
    LocalizationSystem loc;
    EXPECT_FALSE(loc.IsRTLLanguage("en"));
}

// 测试 本地化系统：无参数当获取文本带参数Returnsdefault
TEST(LocalizationSystemTest, WithoutParametersWhenGetTextWithParamsReturnsdefault) {
    LocalizationSystem loc;
    std::unordered_map<std::string, std::string> params;
    EXPECT_EQ(loc.GetTextWithParams("missing.key", params, "Hello"), "Hello");
}

// 测试 本地化系统：带有参数获取
TEST(LocalizationSystemTest, BringParametersAcquire) {
    LocalizationSystem loc;
    std::unordered_map<std::string, std::string> params = {{"name", "Player"}};
    EXPECT_EQ(loc.GetTextWithParams("missing.key", params, "Hello {name}"), "Hello Player");
}

// 测试 本地化系统：注册且
TEST(LocalizationSystemTest, RegisterAnd) {
    LocalizationSystem loc;
    std::string received_lang;
    int cb_id = loc.OnLanguageChanged([&](const std::string& lang) {
        received_lang = lang;
    });
    EXPECT_GE(cb_id, 0);
    loc.UnregisterLanguageChangeCallback(cb_id);
}

// 测试 本地化系统：清空数据
TEST(LocalizationSystemTest, ClearClearData) {
    LocalizationSystem loc;
    loc.Clear();
    EXPECT_EQ(loc.GetCurrentLanguage(), "en");
    EXPECT_TRUE(loc.GetAvailableLanguages().empty());
}
