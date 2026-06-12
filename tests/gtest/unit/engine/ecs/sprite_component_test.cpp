/**
 * @file sprite_component_test.cpp
 * @brief Sprite 相关组件数据结构的单元测试
 *
 * 覆盖场景：
 * - SpriteBlendMode 枚举值
 * - MaterialInstanceComponent 默认值
 * - SpriteRendererComponent 默认值与字段修改
 * - SpineRendererComponent 默认值
 */

#include <gtest/gtest.h>
#include <entt/entt.hpp>
#include "engine/ecs/sprite.h"

// ============================================================
// SpriteBlendMode
// ============================================================

// 测试 精灵混合模式：枚举值正确
TEST(SpriteBlendModeTest, EnumerationValueCorrect) {
    EXPECT_EQ(static_cast<int>(SpriteBlendMode::Alpha), 0);
    EXPECT_EQ(static_cast<int>(SpriteBlendMode::Additive), 1);
    EXPECT_EQ(static_cast<int>(SpriteBlendMode::Multiply), 2);
}

// ============================================================
// MaterialInstanceComponent
// ============================================================

// 测试 材质实例组件：默认值
TEST(MaterialInstanceComponentTest, DefaultValues) {
    MaterialInstanceComponent mat;
    EXPECT_EQ(mat.material_id, 0u);
    EXPECT_EQ(mat.shader_variant, "SPRITE_UNLIT");
    EXPECT_EQ(mat.blend_mode, SpriteBlendMode::Alpha);
    EXPECT_EQ(mat.texture_handle, 0u);
    EXPECT_FLOAT_EQ(mat.tint.r, 1.0f);
    EXPECT_FLOAT_EQ(mat.tint.g, 1.0f);
    EXPECT_FLOAT_EQ(mat.tint.b, 1.0f);
    EXPECT_FLOAT_EQ(mat.tint.a, 1.0f);
}

// 测试 材质实例组件：能够修订
TEST(MaterialInstanceComponentTest, CanRevise) {
    MaterialInstanceComponent mat;
    mat.blend_mode = SpriteBlendMode::Additive;
    mat.texture_handle = 42;
    EXPECT_EQ(mat.blend_mode, SpriteBlendMode::Additive);
    EXPECT_EQ(mat.texture_handle, 42u);
}

// ============================================================
// SpriteRendererComponent
// ============================================================

// 测试 精灵渲染器组件：默认值
TEST(SpriteRendererComponentTest, DefaultValues) {
    SpriteRendererComponent sprite;
    EXPECT_EQ(sprite.texture_handle, 0u);
    EXPECT_EQ(sprite.material_instance_id, 0u);
    EXPECT_EQ(sprite.blend_mode, SpriteBlendMode::Alpha);
    EXPECT_EQ(sprite.sorting_layer, 0);
    EXPECT_EQ(sprite.order_in_layer, 0);
    EXPECT_TRUE(sprite.visible);
    EXPECT_FLOAT_EQ(sprite.color.r, 1.0f);
    EXPECT_FLOAT_EQ(sprite.uv.x, 0.0f);
    EXPECT_FLOAT_EQ(sprite.uv.z, 1.0f);
    EXPECT_FLOAT_EQ(sprite.uv_scroll_speed.x, 0.0f);
}

// 测试 精灵渲染器组件：Revisesorting Andblend
TEST(SpriteRendererComponentTest, RevisesortingAndblend) {
    SpriteRendererComponent sprite;
    sprite.sorting_layer = 5;
    sprite.order_in_layer = 10;
    sprite.blend_mode = SpriteBlendMode::Multiply;
    EXPECT_EQ(sprite.sorting_layer, 5);
    EXPECT_EQ(sprite.order_in_layer, 10);
    EXPECT_EQ(sprite.blend_mode, SpriteBlendMode::Multiply);
}

// ============================================================
// SpineRendererComponent
// ============================================================

// 测试 Spine渲染器组件：默认值
TEST(SpineRendererComponentTest, DefaultValues) {
    SpineRendererComponent spine;
    EXPECT_TRUE(spine.skeleton_data_path.empty());
    EXPECT_EQ(spine.sorting_layer, 0);
    EXPECT_TRUE(spine.visible);
    EXPECT_FLOAT_EQ(spine.time_scale, 1.0f);
    EXPECT_TRUE(spine.loop);
    EXPECT_FALSE(spine.dirty_animation);
}
