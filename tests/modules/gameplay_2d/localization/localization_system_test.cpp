/**
 * @file localization_system_test.cpp
 * @brief 国际化系统单元测试
 */

#include "catch/catch.hpp"
#include "modules/gameplay_2d/localization/localization_system.h"

using namespace dse::gameplay2d;

TEST_CASE("LocalizationSystem - Basic Operations", "[localization]") {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    loc.Clear();
    
    SECTION("Get text with no language loaded") {
        std::string result = loc.GetText("test.key", "default");
        REQUIRE(result == "default");
    }
    
    SECTION("Set current language") {
        // 即使没有加载语言数据，也应该能设置（但会返回 false）
        bool result = loc.SetCurrentLanguage("en");
        REQUIRE(result == false);
    }
}

TEST_CASE("LocalizationSystem - Language Management", "[localization]") {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    loc.Clear();
    
    SECTION("Get available languages") {
        std::vector<std::string> langs = loc.GetAvailableLanguages();
        REQUIRE(langs.empty());
    }
    
    SECTION("Get current language") {
        std::string current = loc.GetCurrentLanguage();
        REQUIRE(current == "en");
    }
}

TEST_CASE("LocalizationSystem - Text Direction Detection", "[localization]") {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    
    SECTION("Detect LTR text") {
        TextDirection dir = loc.DetectTextDirection("Hello World");
        REQUIRE(dir == TextDirection::LTR);
    }
    
    SECTION("Detect RTL language") {
        bool is_rtl = loc.IsRTLLanguage("ar");
        REQUIRE(is_rtl == true);
        
        is_rtl = loc.IsRTLLanguage("en");
        REQUIRE(is_rtl == false);
    }
}

TEST_CASE("LocalizationSystem - Parameter Replacement", "[localization]") {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    loc.Clear();
    
    SECTION("Replace single parameter") {
        std::unordered_map<std::string, std::string> params = {{"name", "Alice"}};
        std::string result = loc.GetTextWithParams("greeting", params, "Hello {name}");
        REQUIRE(result == "Hello Alice");
    }
    
    SECTION("Replace multiple parameters") {
        std::unordered_map<std::string, std::string> params = {
            {"first", "John"},
            {"last", "Doe"}
        };
        std::string result = loc.GetTextWithParams("fullname", params, "{first} {last}");
        REQUIRE(result == "John Doe");
    }
    
    SECTION("Replace repeated parameters") {
        std::unordered_map<std::string, std::string> params = {{"item", "apple"}};
        std::string result = loc.GetTextWithParams("items", params, "I have {item} and {item}");
        REQUIRE(result == "I have apple and apple");
    }
}

TEST_CASE("LocalizationSystem - Callbacks", "[localization]") {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    loc.Clear();
    
    SECTION("Register and unregister callbacks") {
        bool callback_called = false;
        std::string callback_lang;
        
        int callback_id = loc.OnLanguageChanged([&](const std::string& lang) {
            callback_called = true;
            callback_lang = lang;
        });
        
        REQUIRE(callback_id >= 0);
        
        loc.UnregisterLanguageChangeCallback(callback_id);
    }
}

TEST_CASE("LocalizationSystem - RTL Language Detection", "[localization]") {
    LocalizationSystem& loc = LocalizationSystem::GetInstance();
    
    SECTION("Check RTL languages") {
        REQUIRE(loc.IsRTLLanguage("ar") == true);  // Arabic
        REQUIRE(loc.IsRTLLanguage("he") == true);  // Hebrew
        REQUIRE(loc.IsRTLLanguage("fa") == true);  // Farsi
        REQUIRE(loc.IsRTLLanguage("ur") == true);  // Urdu
        REQUIRE(loc.IsRTLLanguage("en") == false); // English
        REQUIRE(loc.IsRTLLanguage("zh") == false); // Chinese
    }
}
