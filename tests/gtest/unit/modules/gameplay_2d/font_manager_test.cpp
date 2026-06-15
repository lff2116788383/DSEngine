/**
 * @file font_manager_test.cpp
 * @brief FontManager 单元测试
 *
 * 覆盖场景：
 * - 字体注册 / 查询 / 卸载
 * - 默认字体设置
 * - 语言-字体映射
 * - 字体回退链
 * - 清空操作
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/localization/font_manager.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace dse::gameplay2d;

class FontManagerTest : public ::testing::Test {
protected:
    FontManager mgr;
    std::vector<std::string> temp_files_;

    // 写一个带 sfnt(TrueType) 魔数头的临时字体文件，返回路径。
    // 用于验证 LoadFont 真实读取磁盘字节（而非仅置位）。
    std::string WriteTempFont(const std::string& name) {
        namespace fs = std::filesystem;
        fs::path p = fs::temp_directory_path() / name;
        std::ofstream out(p, std::ios::binary);
        const unsigned char magic[] = {0x00, 0x01, 0x00, 0x00, 'D', 'S', 'E', 'F'};
        out.write(reinterpret_cast<const char*>(magic), sizeof(magic));
        std::vector<char> pad(64, 0);
        out.write(pad.data(), static_cast<std::streamsize>(pad.size()));
        out.close();
        temp_files_.push_back(p.string());
        return p.string();
    }

    void TearDown() override {
        for (const auto& f : temp_files_) {
            std::error_code ec;
            std::filesystem::remove(f, ec);
        }
    }
};

// 测试 字体管理器：注册字体Success
TEST_F(FontManagerTest, RegisterFont_Success) {
    EXPECT_TRUE(mgr.RegisterFont("main", "data/fonts/main.ttf", 24));
    const auto& cmgr = mgr;
    auto* font = cmgr.GetFont("main");
    ASSERT_NE(font, nullptr);
    EXPECT_EQ(font->font_id, "main");
    EXPECT_EQ(font->font_path, "data/fonts/main.ttf");
    EXPECT_EQ(font->font_size, 24);
    EXPECT_FALSE(font->is_loaded);
}

// 测试 字体管理器：注册字体重复Rejected
TEST_F(FontManagerTest, RegisterFont_DuplicateRejected) {
    EXPECT_TRUE(mgr.RegisterFont("main", "path1.ttf", 16));
    EXPECT_FALSE(mgr.RegisterFont("main", "path2.ttf", 32));
    auto* font = mgr.GetFont("main");
    ASSERT_NE(font, nullptr);
    EXPECT_EQ(font->font_path, "path1.ttf");
    EXPECT_EQ(font->font_size, 16);
}

// 测试 字体管理器：获取字体非存在
TEST_F(FontManagerTest, GetFont_NonExistent) {
    EXPECT_EQ(mgr.GetFont("missing"), nullptr);
    const auto& cmgr = mgr;
    EXPECT_EQ(cmgr.GetFont("missing"), nullptr);
}

// 测试 字体管理器：卸载字体之后加载
TEST_F(FontManagerTest, UnloadFont_AfterLoad) {
    std::string path = WriteTempFont("dse_test_temp.ttf");
    mgr.RegisterFont("temp", path);
    EXPECT_TRUE(mgr.LoadFont("temp"));
    auto* font = mgr.GetFont("temp");
    ASSERT_NE(font, nullptr);
    EXPECT_TRUE(font->is_loaded);
    EXPECT_NE(font->font_data, nullptr);
    mgr.UnloadFont("temp");
    // GetFont auto-loads, so check via const path
    const auto& cmgr = mgr;
    auto* cfont = cmgr.GetFont("temp");
    ASSERT_NE(cfont, nullptr);
    EXPECT_FALSE(cfont->is_loaded);
    EXPECT_EQ(cfont->font_data, nullptr);
}

// 测试 字体管理器：卸载字体Never已加载Noop
TEST_F(FontManagerTest, UnloadFont_NeverLoaded_Noop) {
    mgr.RegisterFont("temp", "temp.ttf");
    mgr.UnloadFont("temp"); // is_loaded=false, noop
    const auto& cmgr = mgr;
    auto* cfont = cmgr.GetFont("temp");
    ASSERT_NE(cfont, nullptr);
    EXPECT_FALSE(cfont->is_loaded);
}

// 测试 字体管理器：获取字体自动加载
TEST_F(FontManagerTest, GetFont_AutoLoads) {
    std::string path = WriteTempFont("dse_test_auto.ttf");
    mgr.RegisterFont("auto", path);
    auto* font = mgr.GetFont("auto"); // non-const triggers auto-load
    ASSERT_NE(font, nullptr);
    EXPECT_TRUE(font->is_loaded);
    EXPECT_NE(font->font_data, nullptr);
}

// 测试 字体管理器：加载字体Real文件Populates数据
TEST_F(FontManagerTest, LoadFont_RealFile_PopulatesData) {
    std::string path = WriteTempFont("dse_test_real.ttf");
    mgr.RegisterFont("real", path);
    EXPECT_TRUE(mgr.LoadFont("real"));
    const auto& cmgr = mgr;
    auto* font = cmgr.GetFont("real");
    ASSERT_NE(font, nullptr);
    EXPECT_TRUE(font->is_loaded);
    EXPECT_NE(font->font_data, nullptr);
}

// 测试 字体管理器：加载字体缺失文件失败
TEST_F(FontManagerTest, LoadFont_MissingFile_Fails) {
    mgr.RegisterFont("ghost", "does/not/exist_dse_xyz.ttf");
    EXPECT_FALSE(mgr.LoadFont("ghost"));
    const auto& cmgr = mgr;
    auto* font = cmgr.GetFont("ghost");
    ASSERT_NE(font, nullptr);
    EXPECT_FALSE(font->is_loaded);
    EXPECT_EQ(font->font_data, nullptr);
}

// 测试 字体管理器：加载字体空路径失败
TEST_F(FontManagerTest, LoadFont_EmptyPath_Fails) {
    mgr.RegisterFont("nopath", "");
    EXPECT_FALSE(mgr.LoadFont("nopath"));
    const auto& cmgr = mgr;
    auto* font = cmgr.GetFont("nopath");
    ASSERT_NE(font, nullptr);
    EXPECT_FALSE(font->is_loaded);
}

// 测试 字体管理器：设置默认字体
TEST_F(FontManagerTest, SetDefaultFont) {
    mgr.RegisterFont("custom", "custom.ttf");
    EXPECT_TRUE(mgr.SetDefaultFont("custom"));
    EXPECT_EQ(mgr.GetDefaultFont(), "custom");
}

// 测试 字体管理器：设置默认字体非存在
TEST_F(FontManagerTest, SetDefaultFont_NonExistent) {
    EXPECT_FALSE(mgr.SetDefaultFont("nonexistent"));
}

// 测试 字体管理器：Language字体映射
TEST_F(FontManagerTest, LanguageFontMapping) {
    mgr.RegisterFont("cn_font", "cn.ttf");
    EXPECT_TRUE(mgr.SetFontForLanguage("zh", "cn_font"));
    EXPECT_EQ(mgr.GetFontForLanguage("zh"), "cn_font");
}

// 测试 字体管理器：获取字体Language回退
TEST_F(FontManagerTest, GetFontForLanguage_Fallback) {
    EXPECT_EQ(mgr.GetFontForLanguage("unknown"), mgr.GetDefaultFont());
}

// 测试 字体管理器：字体回退链
TEST_F(FontManagerTest, FontFallbackChain) {
    mgr.RegisterFont("primary", "primary.ttf");
    mgr.RegisterFont("fallback1", "fb1.ttf");
    mgr.RegisterFont("fallback2", "fb2.ttf");
    mgr.AddFontFallback("primary", "fallback1");
    mgr.AddFontFallback("primary", "fallback2");
    auto chain = mgr.GetFontFallbacks("primary");
    EXPECT_EQ(chain.size(), 2u);
    EXPECT_EQ(chain[0], "fallback1");
    EXPECT_EQ(chain[1], "fallback2");
}

// 测试 字体管理器：获取字体Fallbacks空
TEST_F(FontManagerTest, GetFontFallbacks_Empty) {
    auto chain = mgr.GetFontFallbacks("nope");
    EXPECT_TRUE(chain.empty());
}

// 测试 字体管理器：获取全部字体ID
TEST_F(FontManagerTest, GetAllFontIds) {
    mgr.RegisterFont("a", "a.ttf");
    mgr.RegisterFont("b", "b.ttf");
    auto ids = mgr.GetAllFontIds();
    EXPECT_EQ(ids.size(), 2u);
}

// 测试 字体管理器：清空
TEST_F(FontManagerTest, Clear) {
    mgr.RegisterFont("x", "x.ttf");
    mgr.SetFontForLanguage("en", "x");
    mgr.AddFontFallback("x", "y");
    mgr.Clear();
    EXPECT_EQ(mgr.GetFont("x"), nullptr);
    EXPECT_TRUE(mgr.GetAllFontIds().empty());
}
