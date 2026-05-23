#include <gtest/gtest.h>
#include "engine/render/font/truetype_font.h"

using namespace dse::render;

// TrueTypeFont 测试需要 TTF 文件；此处仅测试 API 语义和空/无效输入处理

TEST(TrueTypeFontTest, DefaultState) {
    TrueTypeFont font;
    EXPECT_FALSE(font.IsValid());
    EXPECT_EQ(font.GetAtlasWidth(), 0);
    EXPECT_EQ(font.GetAtlasHeight(), 0);
    EXPECT_FLOAT_EQ(font.GetFontSize(), 0.0f);
    EXPECT_FLOAT_EQ(font.GetLineHeight(), 0.0f);
    EXPECT_TRUE(font.GetGlyphs().empty());
}

TEST(TrueTypeFontTest, LoadFromMemory_EmptyData) {
    TrueTypeFont font;
    std::vector<uint8_t> empty;
    EXPECT_FALSE(font.LoadFromMemory(empty));
    EXPECT_FALSE(font.IsValid());
}

TEST(TrueTypeFontTest, LoadFromMemory_GarbageData) {
    TrueTypeFont font;
    std::vector<uint8_t> garbage = {0x00, 0x01, 0x02, 0x03, 0xFF};
    EXPECT_FALSE(font.LoadFromMemory(garbage));
    EXPECT_FALSE(font.IsValid());
}

TEST(TrueTypeFontTest, LoadFromFile_NonExistent) {
    TrueTypeFont font;
    EXPECT_FALSE(font.LoadFromFile("non_existent_file.ttf"));
    EXPECT_FALSE(font.IsValid());
}

TEST(TrueTypeFontTest, GetGlyph_InvalidCodepoint) {
    TrueTypeFont font;
    EXPECT_EQ(font.GetGlyph(65), nullptr);
}

TEST(TrueTypeFontTest, MeasureTextWidth_EmptyFont) {
    TrueTypeFont font;
    EXPECT_FLOAT_EQ(font.MeasureTextWidth("hello"), 0.0f);
}

TEST(TrueTypeFontTest, LayoutText_EmptyFont) {
    TrueTypeFont font;
    auto layout = font.LayoutText("hello");
    EXPECT_EQ(layout.size(), 5u);
    for (auto& cl : layout) {
        EXPECT_FLOAT_EQ(cl.x, 0.0f);
    }
}

TEST(TrueTypeFontTest, MoveSemantics) {
    TrueTypeFont a;
    TrueTypeFont b = std::move(a);
    EXPECT_FALSE(b.IsValid());
}

TEST(TrueTypeFontTest, FontAtlasConfig_Defaults) {
    FontAtlasConfig config;
    EXPECT_FLOAT_EQ(config.font_size, 32.0f);
    EXPECT_EQ(config.atlas_width, 1024);
    EXPECT_EQ(config.atlas_height, 1024);
    EXPECT_EQ(config.first_codepoint, 32);
    EXPECT_EQ(config.num_codepoints, 95);
    EXPECT_TRUE(config.extra_codepoints.empty());
}

TEST(TrueTypeFontTest, GlyphMetrics_Defaults) {
    GlyphMetrics gm;
    EXPECT_EQ(gm.codepoint, 0);
    EXPECT_FLOAT_EQ(gm.advance_x, 0.0f);
    EXPECT_FLOAT_EQ(gm.width, 0.0f);
    EXPECT_FLOAT_EQ(gm.height, 0.0f);
}
