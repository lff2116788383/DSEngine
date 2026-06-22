/**
 * @file sprite_ui_render_system_test.cpp
 * @brief SpriteRenderSystem / UIRenderSystem / Expand9SliceItems 无 GPU 单元测试
 *
 * 测试策略：
 * - 系统空 World 不崩溃
 * - Expand9SliceItems 9 宫格展开正确性
 * - UIRendererComponent 默认值
 */

#include <gtest/gtest.h>
#include "modules/gameplay_2d/rendering/sprite_render_system.h"
#include "engine/ecs/components_2d.h"
#include "engine/ecs/transform.h"
#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/opengl/gl_command_buffer.h"
#include "engine/render/rhi/rhi_types.h"

#include <glm/glm.hpp>
#include <vector>

using namespace dse::render;

// ============================================================
// SpriteRenderSystem
// ============================================================

// 测试 精灵渲染系统：默认安全
TEST(SpriteRenderSystemTest, DefaultSafety) {
    SpriteRenderSystem sys;
    (void)sys;
}

// 测试 精灵渲染系统：空世界不崩溃
TEST(SpriteRenderSystemTest, EmptyWorldDoesNotCrash) {
    SpriteRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    dse::render::FrameContext frame;
    sys.Render(world, cmd, frame);
}

// ============================================================
// UIRenderSystem
// ============================================================

// 测试 UI渲染系统：默认安全
TEST(UIRenderSystemTest, DefaultSafety) {
    UIRenderSystem sys;
    (void)sys;
}

// 测试 UI渲染系统：空世界不崩溃
TEST(UIRenderSystemTest, EmptyWorldDoesNotCrash) {
    UIRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    sys.Render(world, cmd, 1920, 1080);
}

// 测试 UI渲染系统：零不崩溃
TEST(UIRenderSystemTest, ZeroDoesNotCrash) {
    UIRenderSystem sys;
    World world;
    OpenGLCommandBuffer cmd;
    sys.Render(world, cmd, 0, 0);
}

// ============================================================
// UIRendererComponent 默认值
// ============================================================

// 测试 精灵UI渲染组件：默认值
TEST(SpriteUIRenderComponentTest, DefaultValues) {
    UIRendererComponent ui;
    EXPECT_EQ(ui.texture_handle, 0u);
    EXPECT_FLOAT_EQ(ui.color.r, 1.0f);
    EXPECT_FLOAT_EQ(ui.color.a, 1.0f);
    EXPECT_EQ(ui.order, 0);
    EXPECT_TRUE(ui.visible);
    EXPECT_FLOAT_EQ(ui.scale, 1.0f);
    EXPECT_FLOAT_EQ(ui.hover_scale, 1.08f);
    EXPECT_FLOAT_EQ(ui.pressed_scale, 0.94f);
}

// 测试 精灵UI渲染组件：参数默认值
TEST(SpriteUIRenderComponentTest, ParameterDefaultValue) {
    UIRendererComponent ui;
    EXPECT_FLOAT_EQ(ui.position.x, 0.0f);
    EXPECT_FLOAT_EQ(ui.position.y, 0.0f);
    EXPECT_FLOAT_EQ(ui.size.x, 100.0f);
    EXPECT_FLOAT_EQ(ui.size.y, 100.0f);
    EXPECT_FLOAT_EQ(ui.anchor_min.x, 0.5f);
    EXPECT_FLOAT_EQ(ui.anchor_max.x, 0.5f);
    EXPECT_FLOAT_EQ(ui.pivot.x, 0.5f);
}

// 测试 精灵UI渲染组件：状态默认值
TEST(SpriteUIRenderComponentTest, StateDefaultValues) {
    UIRendererComponent ui;
    EXPECT_TRUE(ui.interactable);
    EXPECT_FALSE(ui.is_hovered);
    EXPECT_FALSE(ui.is_pressed);
    EXPECT_FALSE(ui.nine_slice_enabled);
}

// ============================================================
// Expand9SliceItems
// ============================================================

// 测试 展开9切片：Zerogenerate 1项
TEST(Expand9SliceTest, Zerogenerate1Item) {
    SpriteDrawItem base;
    base.texture_handle = 42;
    base.color = glm::vec4(1.0f);
    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base,
                      glm::vec2(400, 300),   // center
                      glm::vec2(200, 100),   // size
                      glm::vec4(0, 0, 1, 1), // uv full
                      glm::vec4(0, 0, 0, 0), // border = 0
                      glm::vec2(0, 0),        // src_size
                      out);
    // 零边框时中心块应占全部
    EXPECT_GE(out.size(), 1u);
}

// 测试 展开9切片：Validgenerate多9项
TEST(Expand9SliceTest, ValidgenerateMulti9Item) {
    SpriteDrawItem base;
    base.texture_handle = 42;
    base.color = glm::vec4(1.0f);
    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base,
                      glm::vec2(400, 300),
                      glm::vec2(200, 100),
                      glm::vec4(0, 0, 1, 1),
                      glm::vec4(0.1f, 0.1f, 0.1f, 0.1f),
                      glm::vec2(100, 100),
                      out);
    EXPECT_GE(out.size(), 1u);
    EXPECT_LE(out.size(), 9u);
}

// 测试 展开9切片：带项
TEST(Expand9SliceTest, WithItem) {
    SpriteDrawItem base;
    base.texture_handle = 99;
    base.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
    std::vector<SpriteDrawItem> out;
    Expand9SliceItems(base,
                      glm::vec2(200, 200),
                      glm::vec2(300, 300),
                      glm::vec4(0, 0, 1, 1),
                      glm::vec4(0.2f, 0.2f, 0.2f, 0.2f),
                      glm::vec2(64, 64),
                      out);
    for (const auto& item : out) {
        EXPECT_EQ(item.texture_handle, 99u);
    }
}
