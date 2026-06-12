/**
* @file localization_system_test.cpp
* @brief 国际化系统单元测试，验证多语言文本查找、参数替换、RTL 检测、回调等核心功能
*/

#include <gtest/gtest.h>
#include "modules/gameplay_2d/localization/localization_system.h"

using namespace dse::gameplay2d;

TEST(LocalizationSystemTest, DefaultIsen) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetCurrentLanguage(), "en");
}

TEST(LocalizationSystemTest, DoesNotExistWhenReturnsdefault) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetText("missing.key", "fallback"), "fallback");
}

TEST(LocalizationSystemTest, DoesNotExistWithoutdefaultReturnsEmpty) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.GetText("missing.key"), "");
}

TEST(LocalizationSystemTest, SetUpNotLoadReturnsFails) {
    LocalizationSystem loc;
    EXPECT_FALSE(loc.SetCurrentLanguage("zh"));
}

TEST(LocalizationSystemTest, DefaultWithoutAlreadyLoad) {
    LocalizationSystem loc;
    auto langs = loc.GetAvailableLanguages();
    EXPECT_TRUE(langs.empty());
}

TEST(LocalizationSystemTest, IsLTRToward) {
    LocalizationSystem loc;
    EXPECT_EQ(loc.DetectTextDirection("Hello"), TextDirection::LTR);
}

TEST(LocalizationSystemTest, IsRTL) {
    LocalizationSystem loc;
    EXPECT_TRUE(loc.IsRTLLanguage("ar"));
}

TEST(LocalizationSystemTest, NotRTL) {
    LocalizationSystem loc;
    EXPECT_FALSE(loc.IsRTLLanguage("en"));
}

TEST(LocalizationSystemTest, WithoutParametersWhenGetTextWithParamsReturnsdefault) {
    LocalizationSystem loc;
    std::unordered_map<std::string, std::string> params;
    EXPECT_EQ(loc.GetTextWithParams("missing.key", params, "Hello"), "Hello");
}

TEST(LocalizationSystemTest, BringParametersAcquire) {
    LocalizationSystem loc;
    std::unordered_map<std::string, std::string> params = {{"name", "Player"}};
    EXPECT_EQ(loc.GetTextWithParams("missing.key", params, "Hello {name}"), "Hello Player");
}

TEST(LocalizationSystemTest, RegisterAnd) {
    LocalizationSystem loc;
    std::string received_lang;
    int cb_id = loc.OnLanguageChanged([&](const std::string& lang) {
        received_lang = lang;
    });
    EXPECT_GE(cb_id, 0);
    loc.UnregisterLanguageChangeCallback(cb_id);
}

TEST(LocalizationSystemTest, ClearClearData) {
    LocalizationSystem loc;
    loc.Clear();
    EXPECT_EQ(loc.GetCurrentLanguage(), "en");
    EXPECT_TRUE(loc.GetAvailableLanguages().empty());
}
