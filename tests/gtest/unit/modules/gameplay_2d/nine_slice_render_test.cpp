/**
 * @file nine_slice_render_test.cpp
 * @brief Expand9SliceItems 展开逻辑单元测试
 *
 * 覆盖场景：
 * - 标准 9 宫格展开产生 9 个 DrawItem
 * - 退化边框（0 边框）整体作为单格输出 1 个 DrawItem
 * - 角块 UV 与屏幕位置计算正确性
 * - 尺寸不足导致格子跳过（退化宽/高）
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include <glm/glm.hpp>

static SpriteDrawItem MakeBase() {
    SpriteDrawItem item;
    item.texture_handle = 42u;
    item.sorting_layer = 1000;
    item.order_in_layer = 5;
    item.color = glm::vec4(1.0f);
    return item;
}

// ============================================================
// 等比缩放模式（src_size = 0,0）
// ============================================================

// 测试 展开9切片：情形9绘制项
TEST(Expand9SliceTest, Case9DrawItem) {
    SpriteDrawItem base = MakeBase();
    glm::vec2 center(200.0f, 100.0f);
    glm::vec2 size(200.0f, 100.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.1f, 0.1f, 0.1f, 0.1f);

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base, center, size, uv, border, glm::vec2(0.0f), out);

    EXPECT_EQ(out.size(), 9u);
}

// ============================================================
// 全零边框：左/右列和上/下行退化，只有中心格存活
// ============================================================

// 测试 展开9切片：零于有效
TEST(Expand9SliceTest, ZeroInValid) {
    SpriteDrawItem base = MakeBase();
    glm::vec2 center(100.0f, 100.0f);
    glm::vec2 size(100.0f, 100.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.0f, 0.0f, 0.0f, 0.0f);

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base, center, size, uv, border, glm::vec2(0.0f), out);

    // 零边框：左/右列宽=0 → col=0,2 跳过；上/下行高=0 → row=0,2 跳过
    // 只有中心格 (col=1, row=1) 存活，输出 1 个 DrawItem
    EXPECT_EQ(out.size(), 1u);
}

// ============================================================
// 角块 UV 校验（左下角 col=0,row=0）
// ============================================================

// 测试 展开9切片：UV正确
TEST(Expand9SliceTest, UVCorrect) {
    SpriteDrawItem base = MakeBase();
    glm::vec2 center(150.0f, 75.0f);
    glm::vec2 size(300.0f, 150.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.2f, 0.2f, 0.2f, 0.2f);

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base, center, size, uv, border, glm::vec2(0.0f), out);
    ASSERT_EQ(out.size(), 9u);

    const SpriteDrawItem& bl = out[0];  // col=0, row=0
    EXPECT_FLOAT_EQ(bl.uv.x, 0.0f);
    EXPECT_FLOAT_EQ(bl.uv.y, 0.0f);
    EXPECT_NEAR(bl.uv.z, 0.2f, 1e-5f);
    EXPECT_NEAR(bl.uv.w, 0.2f, 1e-5f);
}

// ============================================================
// 右上角 UV 校验（col=2, row=2）
// ============================================================

// 测试 展开9切片：UV正确2
TEST(Expand9SliceTest, UVCorrect_2) {
    SpriteDrawItem base = MakeBase();
    glm::vec2 center(0.0f, 0.0f);
    glm::vec2 size(100.0f, 100.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.25f, 0.25f, 0.25f, 0.25f);

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base, center, size, uv, border, glm::vec2(0.0f), out);
    ASSERT_EQ(out.size(), 9u);

    const SpriteDrawItem& tr = out[2 * 3 + 2];  // col=2, row=2
    EXPECT_NEAR(tr.uv.x, 0.75f, 1e-5f);
    EXPECT_NEAR(tr.uv.y, 0.75f, 1e-5f);
    EXPECT_NEAR(tr.uv.z, 0.25f, 1e-5f);
    EXPECT_NEAR(tr.uv.w, 0.25f, 1e-5f);
}

// ============================================================
// 屏幕尺寸校验：各格宽高之和应等于 widget 总尺寸（等比模式）
// ============================================================

// 测试 展开9切片：模型且
TEST(Expand9SliceTest, ModelAnd) {
    SpriteDrawItem base = MakeBase();
    glm::vec2 center(200.0f, 200.0f);
    glm::vec2 size(400.0f, 200.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.15f, 0.2f, 0.15f, 0.2f);

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base, center, size, uv, border, glm::vec2(0.0f), out);
    ASSERT_EQ(out.size(), 9u);

    float total_w = 0.0f;
    for (int col = 0; col < 3; ++col) {
        total_w += out[static_cast<size_t>(col) * 3].model[0][0];
    }
    EXPECT_NEAR(total_w, size.x, 1e-3f);

    float total_h = 0.0f;
    for (int row = 0; row < 3; ++row) {
        total_h += out[static_cast<size_t>(row)].model[1][1];
    }
    EXPECT_NEAR(total_h, size.y, 1e-3f);
}

// ============================================================
// base_item 属性传递（texture_handle / order_in_layer）
// ============================================================

// 测试 展开9切片：基正确
TEST(Expand9SliceTest, BaseCorrect) {
    SpriteDrawItem base = MakeBase();
    base.texture_handle = 77u;
    base.order_in_layer = 12;

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base,
                      glm::vec2(50.0f, 50.0f),
                      glm::vec2(100.0f, 100.0f),
                      glm::vec4(0.0f, 0.0f, 1.0f, 1.0f),
                      glm::vec4(0.1f, 0.1f, 0.1f, 0.1f),
                      glm::vec2(0.0f),
                      out);
    ASSERT_FALSE(out.empty());
    for (const auto& item : out) {
        EXPECT_EQ(item.texture_handle, 77u);
        EXPECT_EQ(item.order_in_layer, 12);
    }
}

// ============================================================
// 固定角块模式（src_size > 0）：角块屏幕尺寸 = border × src_size，不随 widget 变化
// ============================================================

// 测试 展开9切片：模型独立
TEST(Expand9SliceTest, ModelIndependent) {
    SpriteDrawItem base = MakeBase();
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.1f, 0.1f, 0.1f, 0.1f);
    glm::vec2 src_size(100.0f, 100.0f);  // 源精灵 100×100，border=0.1 → 角块 10px

    // 测试 widget 从 100px 拉伸到 400px
    std::vector<SpriteDrawItem> out_small, out_large;
    Expand9SliceItems(base, glm::vec2(50.0f, 50.0f),  glm::vec2(100.0f, 100.0f), uv, border, src_size, out_small);
    Expand9SliceItems(base, glm::vec2(200.0f, 50.0f), glm::vec2(400.0f, 100.0f), uv, border, src_size, out_large);

    ASSERT_EQ(out_small.size(), 9u);
    ASSERT_EQ(out_large.size(), 9u);

    // col=0（左列）的宽度在两种 widget 尺寸下应相同
    float corner_w_small = out_small[0].model[0][0];  // col=0, row=0
    float corner_w_large = out_large[0].model[0][0];
    EXPECT_NEAR(corner_w_small, 10.0f, 1e-3f);  // 0.1 × 100 = 10px
    EXPECT_NEAR(corner_w_large, 10.0f, 1e-3f);  // 固定，不随 widget 变化
}

// 测试 展开9切片：模型仍
TEST(Expand9SliceTest, ModelStill) {
    SpriteDrawItem base = MakeBase();
    glm::vec2 size(500.0f, 80.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.1f, 0.2f, 0.1f, 0.2f);
    glm::vec2 src_size(100.0f, 50.0f);  // 角块: left=10, right=10, bottom=10, top=10

    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base, glm::vec2(250.0f, 40.0f), size, uv, border, src_size, out);
    ASSERT_EQ(out.size(), 9u);

    float total_w = 0.0f;
    for (int col = 0; col < 3; ++col) {
        total_w += out[static_cast<size_t>(col) * 3].model[0][0];
    }
    EXPECT_NEAR(total_w, size.x, 1e-3f);

    float total_h = 0.0f;
    for (int row = 0; row < 3; ++row) {
        total_h += out[static_cast<size_t>(row)].model[1][1];
    }
    EXPECT_NEAR(total_h, size.y, 1e-3f);
}

// 测试 展开9切片：模型UV Andmodel
TEST(Expand9SliceTest, ModelUVAndmodel) {
    // UV 分割点由 border UV 分量决定，与 src_size 无关
    SpriteDrawItem base = MakeBase();
    glm::vec2 size(300.0f, 100.0f);
    glm::vec4 uv(0.0f, 0.0f, 1.0f, 1.0f);
    glm::vec4 border(0.2f, 0.2f, 0.2f, 0.2f);

    std::vector<SpriteDrawItem> prop_out, fixed_out;
    Expand9SliceItems(base, glm::vec2(150.0f, 50.0f), size, uv, border, glm::vec2(0.0f),   prop_out);
    Expand9SliceItems(base, glm::vec2(150.0f, 50.0f), size, uv, border, glm::vec2(100.0f, 100.0f), fixed_out);

    ASSERT_EQ(prop_out.size(),  9u);
    ASSERT_EQ(fixed_out.size(), 9u);
    for (size_t i = 0; i < 9; ++i) {
        EXPECT_NEAR(prop_out[i].uv.x, fixed_out[i].uv.x, 1e-5f);
        EXPECT_NEAR(prop_out[i].uv.y, fixed_out[i].uv.y, 1e-5f);
        EXPECT_NEAR(prop_out[i].uv.z, fixed_out[i].uv.z, 1e-5f);
        EXPECT_NEAR(prop_out[i].uv.w, fixed_out[i].uv.w, 1e-5f);
    }
}
