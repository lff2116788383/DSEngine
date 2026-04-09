/**
 * @file font_manager_test.cpp
 * @brief 字体管理器单元测试
 */

#include "catch/catch.hpp"
#include "modules/gameplay_2d/localization/font_manager.h"

using namespace dse::gameplay2d;

TEST_CASE("FontManager - Basic Operations", "[font_manager]") {
    FontManager& fm = FontManager::GetInstance();
    fm.Clear();
    
    SECTION("Register font") {
        bool result = fm.RegisterFont("test_font", "data/fonts/test.ttf", 32);
        REQUIRE(result == true);
        
        // 重复注册应该失败
        result = fm.RegisterFont("test_font", "data/fonts/test.ttf", 32);
        REQUIRE(result == false);
    }
    
    SECTION("Get font") {
        fm.RegisterFont("test_font", "data/fonts/test.ttf", 32);
        FontAsset* font = fm.GetFont("test_font");
        REQUIRE(font != nullptr);
        REQUIRE(font->font_id == "test_font");
        REQUIRE(font->font_size == 32);
    }
    
    SECTION("Get non-existent font") {
        FontAsset* font = fm.GetFont("non_existent");
        // 应该返回默认字体或 nullptr
        // 取决于是否注册了默认字体
    }
}

TEST_CASE("FontManager - Default Font", "[font_manager]") {
    FontManager& fm = FontManager::GetInstance();
    fm.Clear();
    
    SECTION("Set default font") {
        fm.RegisterFont("default", "data/fonts/default.ttf", 32);
        bool result = fm.SetDefaultFont("default");
        REQUIRE(result == true);
        REQUIRE(fm.GetDefaultFont() == "default");
    }
    
    SECTION("Set non-existent default font") {
        bool result = fm.SetDefaultFont("non_existent");
        REQUIRE(result == false);
    }
}

TEST_CASE("FontManager - Language Font Mapping", "[font_manager]") {
    FontManager& fm = FontManager::GetInstance();
    fm.Clear();
    
    SECTION("Set font for language") {
        fm.RegisterFont("chinese_font", "data/fonts/chinese.ttf", 32);
        bool result = fm.SetFontForLanguage("zh", "chinese_font");
        REQUIRE(result == true);
        
        std::string font_id = fm.GetFontForLanguage("zh");
        REQUIRE(font_id == "chinese_font");
    }
    
    SECTION("Get font for language without mapping") {
        fm.RegisterFont("default", "data/fonts/default.ttf", 32);
        fm.SetDefaultFont("default");
        
        std::string font_id = fm.GetFontForLanguage("unknown");
        REQUIRE(font_id == "default");
    }
}

TEST_CASE("FontManager - Font Fallback", "[font_manager]") {
    FontManager& fm = FontManager::GetInstance();
    fm.Clear();
    
    SECTION("Add font fallback") {
        fm.RegisterFont("primary", "data/fonts/primary.ttf", 32);
        fm.RegisterFont("fallback", "data/fonts/fallback.ttf", 32);
        
        fm.AddFontFallback("primary", "fallback");
        
        std::vector<std::string> fallbacks = fm.GetFontFallbacks("primary");
        REQUIRE(fallbacks.size() == 1);
        REQUIRE(fallbacks[0] == "fallback");
    }
    
    SECTION("Add multiple fallbacks") {
        fm.RegisterFont("primary", "data/fonts/primary.ttf", 32);
        fm.RegisterFont("fallback1", "data/fonts/fallback1.ttf", 32);
        fm.RegisterFont("fallback2", "data/fonts/fallback2.ttf", 32);
        
        fm.AddFontFallback("primary", "fallback1");
        fm.AddFontFallback("primary", "fallback2");
        
        std::vector<std::string> fallbacks = fm.GetFontFallbacks("primary");
        REQUIRE(fallbacks.size() == 2);
    }
}

TEST_CASE("FontManager - Get All Fonts", "[font_manager]") {
    FontManager& fm = FontManager::GetInstance();
    fm.Clear();
    
    SECTION("Get all font IDs") {
        fm.RegisterFont("font1", "data/fonts/font1.ttf", 32);
        fm.RegisterFont("font2", "data/fonts/font2.ttf", 32);
        fm.RegisterFont("font3", "data/fonts/font3.ttf", 32);
        
        std::vector<std::string> font_ids = fm.GetAllFontIds();
        REQUIRE(font_ids.size() == 3);
    }
}

TEST_CASE("FontManager - Clear", "[font_manager]") {
    FontManager& fm = FontManager::GetInstance();
    
    SECTION("Clear all fonts") {
        fm.RegisterFont("font1", "data/fonts/font1.ttf", 32);
        fm.RegisterFont("font2", "data/fonts/font2.ttf", 32);
        
        fm.Clear();
        
        std::vector<std::string> font_ids = fm.GetAllFontIds();
        REQUIRE(font_ids.empty());
    }
}
