#include <gtest/gtest.h>
#include "engine/render/font/truetype_font.h"
#include <filesystem>
#include <fstream>

using namespace dse::render;

// TrueTypeFont 测试需要 TTF 文件；此处仅测试 API 语义和空/无效输入处理

// 测试 真类型字体：默认状态
TEST(TrueTypeFontTest, DefaultState) {
    TrueTypeFont font;
    EXPECT_FALSE(font.IsValid());
    EXPECT_EQ(font.GetAtlasWidth(), 0);
    EXPECT_EQ(font.GetAtlasHeight(), 0);
    EXPECT_FLOAT_EQ(font.GetFontSize(), 0.0f);
    EXPECT_FLOAT_EQ(font.GetLineHeight(), 0.0f);
    EXPECT_TRUE(font.GetGlyphs().empty());
}

// 测试 真类型字体：加载从内存空数据
TEST(TrueTypeFontTest, LoadFromMemory_EmptyData) {
    TrueTypeFont font;
    std::vector<uint8_t> empty;
    EXPECT_FALSE(font.LoadFromMemory(empty));
    EXPECT_FALSE(font.IsValid());
}

// 测试 真类型字体：加载从内存Garbage数据
TEST(TrueTypeFontTest, LoadFromMemory_GarbageData) {
    TrueTypeFont font;
    std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0xFF};
    EXPECT_FALSE(font.LoadFromMemory(garbage));
    EXPECT_FALSE(font.IsValid());
}

// 测试 真类型字体：加载从文件非存在
TEST(TrueTypeFontTest, LoadFromFile_NonExistent) {
    TrueTypeFont font;
    EXPECT_FALSE(font.LoadFromFile("non_existent_file.ttf"));
    EXPECT_FALSE(font.IsValid());
}

// 损坏样本：空文件 tellg 返回 0，旧实现 static_cast<size_t>(-1) 风险路径必须安全拒绝
TEST(TrueTypeFontTest, LoadFromFile_EmptyFileRejected) {
    namespace fs = std::filesystem;
    auto tmp = (fs::temp_directory_path() / "dse_empty_font.ttf").string();
    { std::ofstream out(tmp, std::ios::binary); } // 0 字节
    TrueTypeFont font;
    EXPECT_FALSE(font.LoadFromFile(tmp)); // 不崩溃
    EXPECT_FALSE(font.IsValid());
    fs::remove(tmp);
}

// 测试 真类型字体：获取Glyph无效Codepoint
TEST(TrueTypeFontTest, GetGlyph_InvalidCodepoint) {
    TrueTypeFont font;
    EXPECT_EQ(font.GetGlyph(65), nullptr);
}

// 测试 真类型字体：Measure文本宽度空字体
TEST(TrueTypeFontTest, MeasureTextWidth_EmptyFont) {
    TrueTypeFont font;
    EXPECT_FLOAT_EQ(font.MeasureTextWidth("hello"), 0.0f);
}

// 测试 真类型字体：布局文本空字体
TEST(TrueTypeFontTest, LayoutText_EmptyFont) {
    TrueTypeFont font;
    auto layout = font.LayoutText("hello");
    EXPECT_EQ(layout.size(), 5u);
    for (auto& cl : layout) {
        EXPECT_FLOAT_EQ(cl.x, 0.0f);
    }
}

// 测试 真类型字体：移动Semantics
TEST(TrueTypeFontTest, MoveSemantics) {
    TrueTypeFont a;
    TrueTypeFont b = std::move(a);
    EXPECT_FALSE(b.IsValid());
}

// 测试 真类型字体：字体图集配置默认值
TEST(TrueTypeFontTest, FontAtlasConfig_Defaults) {
    FontAtlasConfig config;
    EXPECT_FLOAT_EQ(config.font_size, 32.0f);
    EXPECT_EQ(config.atlas_width, 1024);
    EXPECT_EQ(config.atlas_height, 1024);
    EXPECT_EQ(config.first_codepoint, 32);
    EXPECT_EQ(config.num_codepoints, 95);
    EXPECT_TRUE(config.extra_codepoints.empty());
}

// 测试 真类型字体：Glyph Metrics默认值
TEST(TrueTypeFontTest, GlyphMetrics_Defaults) {
    GlyphMetrics gm;
    EXPECT_EQ(gm.codepoint, 0);
    EXPECT_FLOAT_EQ(gm.advance_x, 0.0f);
    EXPECT_FLOAT_EQ(gm.width, 0.0f);
    EXPECT_FLOAT_EQ(gm.height, 0.0f);
}
