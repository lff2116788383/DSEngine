#include <gtest/gtest.h>
#include "engine/assets/localization_manager.h"

using namespace dse::assets;

class LocalizationManagerTest : public ::testing::Test {
protected:
    LocalizationManager mgr;
};

// 测试 本地化管理器：加载区域设置从字符串有效JSON
TEST_F(LocalizationManagerTest, LoadLocaleFromString_ValidJson) {
    const char* json = R"({"ui.ok": "OK", "ui.cancel": "Cancel"})";
    EXPECT_TRUE(mgr.LoadLocaleFromString(json, "en-US"));
    EXPECT_EQ(mgr.GetEntryCount("en-US"), 2u);
}

// 测试 本地化管理器：加载区域设置从字符串无效JSON
TEST_F(LocalizationManagerTest, LoadLocaleFromString_InvalidJson) {
    EXPECT_FALSE(mgr.LoadLocaleFromString("not json", "bad"));
}

// 测试 本地化管理器：加载区域设置从字符串空对象
TEST_F(LocalizationManagerTest, LoadLocaleFromString_EmptyObject) {
    EXPECT_TRUE(mgr.LoadLocaleFromString("{}", "empty"));
    EXPECT_EQ(mgr.GetEntryCount("empty"), 0u);
}

// 测试 本地化管理器：设置当前区域设置自动设置开启首个加载
TEST_F(LocalizationManagerTest, SetCurrentLocale_AutoSetOnFirstLoad) {
    mgr.LoadLocaleFromString(R"({"k":"v"})", "zh-CN");
    EXPECT_EQ(mgr.GetCurrentLocale(), "zh-CN");
}

// 测试 本地化管理器：设置当前区域设置Explicit切换
TEST_F(LocalizationManagerTest, SetCurrentLocale_ExplicitSwitch) {
    mgr.LoadLocaleFromString(R"({"k":"A"})", "en");
    mgr.LoadLocaleFromString(R"({"k":"B"})", "zh");
    mgr.SetCurrentLocale("zh");
    EXPECT_EQ(mgr.GetCurrentLocale(), "zh");
    EXPECT_EQ(mgr.Get("k"), "B");
}

// 测试 本地化管理器：获取返回键若缺失
TEST_F(LocalizationManagerTest, Get_ReturnsKeyIfMissing) {
    mgr.LoadLocaleFromString(R"({"a":"1"})", "en");
    EXPECT_EQ(mgr.Get("missing_key"), "missing_key");
}

// 测试 本地化管理器：获取无区域设置已加载返回键
TEST_F(LocalizationManagerTest, Get_NoLocaleLoaded_ReturnsKey) {
    EXPECT_EQ(mgr.Get("anything"), "anything");
}

// 测试 本地化管理器：获取带参数
TEST_F(LocalizationManagerTest, Get_WithParams) {
    mgr.LoadLocaleFromString(R"({"greeting": "Hello, {name}! You have {count} msgs."})", "en");
    auto result = mgr.Get("greeting", {{"name", "Alice"}, {"count", "3"}});
    EXPECT_EQ(result, "Hello, Alice! You have 3 msgs.");
}

// 测试 本地化管理器：获取带参数缺失参数
TEST_F(LocalizationManagerTest, Get_WithParams_MissingParam) {
    mgr.LoadLocaleFromString(R"({"t": "Value is {x}"})", "en");
    EXPECT_EQ(mgr.Get("t", {}), "Value is {x}");
}

// 测试 本地化管理器：获取带Escaped Chars
TEST_F(LocalizationManagerTest, Get_WithEscapedChars) {
    mgr.LoadLocaleFromString(R"({"t": "line1\nline2"})", "en");
    EXPECT_EQ(mgr.Get("t"), "line1\nline2");
}

// 测试 本地化管理器：获取区域设置
TEST_F(LocalizationManagerTest, GetForLocale) {
    mgr.LoadLocaleFromString(R"({"k":"EN"})", "en");
    mgr.LoadLocaleFromString(R"({"k":"ZH"})", "zh");
    EXPECT_EQ(mgr.GetForLocale("zh", "k"), "ZH");
    EXPECT_EQ(mgr.GetForLocale("en", "k"), "EN");
    EXPECT_EQ(mgr.GetForLocale("fr", "k"), "");
}

// 测试 本地化管理器：拥有键
TEST_F(LocalizationManagerTest, HasKey) {
    mgr.LoadLocaleFromString(R"({"exist":"yes"})", "en");
    EXPECT_TRUE(mgr.HasKey("exist"));
    EXPECT_FALSE(mgr.HasKey("nope"));
}

// 测试 本地化管理器：获取Available Locales
TEST_F(LocalizationManagerTest, GetAvailableLocales) {
    mgr.LoadLocaleFromString(R"({})", "en");
    mgr.LoadLocaleFromString(R"({})", "zh");
    auto locales = mgr.GetAvailableLocales();
    EXPECT_EQ(locales.size(), 2u);
}

// 测试 本地化管理器：开启区域设置变更回调
TEST_F(LocalizationManagerTest, OnLocaleChanged_Callback) {
    mgr.LoadLocaleFromString(R"({})", "en");
    mgr.LoadLocaleFromString(R"({})", "zh");
    std::string received;
    mgr.OnLocaleChanged([&](const std::string& loc) { received = loc; });
    mgr.SetCurrentLocale("zh");
    EXPECT_EQ(received, "zh");
}

// 测试 本地化管理器：开启区域设置变更不被调用若相同区域设置
TEST_F(LocalizationManagerTest, OnLocaleChanged_NotCalledIfSameLocale) {
    mgr.LoadLocaleFromString(R"({})", "en");
    int call_count = 0;
    mgr.OnLocaleChanged([&](const std::string&) { ++call_count; });
    mgr.SetCurrentLocale("en");
    EXPECT_EQ(call_count, 0);
}
