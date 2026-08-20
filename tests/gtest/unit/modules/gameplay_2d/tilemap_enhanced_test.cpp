/**
 * @file tilemap_enhanced_test.cpp
 * @brief 增强版瓦片地图单元测试
 *
 * 覆盖：
 * - 多层 Tilemap
 * - 动画瓦片 TileAnimation
 * - 瓦片属性 TileProperties
 * - .dtilemap 序列化/反序列化
 * - 单层模式向后兼容
 */

#include <gtest/gtest.h>
#include "engine/ecs/tilemap.h"
#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace dse;
using namespace gameplay2d;

// ── 动画瓦片测试 ────────────────────────────────────────────────────────────

TEST(TilemapAnimationTest, ComputeDuration) {
    TileAnimation anim;
    anim.frames = {{1, 0.2f}, {2, 0.3f}, {3, 0.1f}};
    anim.ComputeDuration();
    EXPECT_FLOAT_EQ(anim.total_duration, 0.6f);
}

TEST(TilemapAnimationTest, GetFrameAt) {
    TileAnimation anim;
    anim.frames = {{1, 0.2f}, {2, 0.3f}, {3, 0.1f}};
    anim.ComputeDuration();

    EXPECT_EQ(anim.GetFrameAt(0.0f), 1);
    EXPECT_EQ(anim.GetFrameAt(0.1f), 1);
    EXPECT_EQ(anim.GetFrameAt(0.2f), 2);
    EXPECT_EQ(anim.GetFrameAt(0.4f), 2);
    EXPECT_EQ(anim.GetFrameAt(0.5f), 3);
    // 循环
    EXPECT_EQ(anim.GetFrameAt(0.61f), 1);  // 0.61 % 0.6 ≈ 0.01 → frame 1
    EXPECT_EQ(anim.GetFrameAt(0.81f), 2);  // 0.81 % 0.6 ≈ 0.21 → frame 2
}

TEST(TilemapAnimationTest, NoLoop) {
    TileAnimation anim;
    anim.frames = {{1, 0.2f}, {2, 0.3f}};
    anim.loop = false;
    anim.ComputeDuration();

    EXPECT_EQ(anim.GetFrameAt(0.1f), 1);
    EXPECT_EQ(anim.GetFrameAt(0.3f), 2);
    // 超过总时长后停在最后一帧
    EXPECT_EQ(anim.GetFrameAt(1.0f), 2);
}

// ── 瓦片属性测试 ────────────────────────────────────────────────────────────

TEST(TilemapPropertiesTest, DefaultValues) {
    TileProperties prop;
    EXPECT_FALSE(prop.solid);
    EXPECT_EQ(prop.collision_type, 0);
    EXPECT_FLOAT_EQ(prop.friction, 0.4f);
    EXPECT_FLOAT_EQ(prop.restitution, 0.0f);
}

TEST(TilemapPropertiesTest, CustomProperties) {
    TileProperties prop;
    prop.Set("damage", "10");
    prop.Set("walkable", "true");

    EXPECT_EQ(prop.Get("damage"), "10");
    EXPECT_EQ(prop.Get("walkable"), "true");
    EXPECT_EQ(prop.Get("nonexistent", "default"), "default");
    EXPECT_TRUE(prop.Has("damage"));
    EXPECT_FALSE(prop.Has("nonexistent"));
}

// ── TilemapComponent 多层测试 ──────────────────────────────────────────────

TEST(TilemapComponentTest, AddLayer) {
    TilemapComponent tm;
    tm.width = 5;
    tm.height = 3;
    tm.tile_size = 1.0f;

    int idx = tm.AddLayer("background");
    EXPECT_EQ(idx, 0);
    EXPECT_EQ(tm.LayerCount(), 1);

    auto* layer = tm.GetLayerMut(0);
    ASSERT_NE(layer, nullptr);
    EXPECT_EQ(layer->name, "background");
    EXPECT_EQ(layer->width, 5);
    EXPECT_EQ(layer->height, 3);
    EXPECT_EQ(layer->tiles.size(), 15u);
    EXPECT_FLOAT_EQ(layer->opacity, 1.0f);
    EXPECT_TRUE(layer->visible);
    EXPECT_TRUE(layer->dirty);
}

TEST(TilemapComponentTest, MultipleLayers) {
    TilemapComponent tm;
    tm.width = 4;
    tm.height = 4;

    tm.AddLayer("bg");
    tm.AddLayer("mid");
    tm.AddLayer("fg");

    EXPECT_EQ(tm.LayerCount(), 3);
    EXPECT_EQ(tm.GetLayer(0)->name, "bg");
    EXPECT_EQ(tm.GetLayer(1)->name, "mid");
    EXPECT_EQ(tm.GetLayer(2)->name, "fg");
    EXPECT_EQ(tm.GetLayer(3), nullptr);  // 超出范围
}

TEST(TilemapComponentTest, MarkAllDirty) {
    TilemapComponent tm;
    tm.width = 2;
    tm.height = 2;
    tm.AddLayer("layer1");
    tm.AddLayer("layer2");

    // 清除 dirty
    tm.layers[0].dirty = false;
    tm.layers[1].dirty = false;
    tm.dirty = false;

    tm.MarkAllDirty();

    EXPECT_TRUE(tm.dirty);
    EXPECT_TRUE(tm.layers[0].dirty);
    EXPECT_TRUE(tm.layers[1].dirty);
}

TEST(TilemapComponentTest, AddAnimation) {
    TilemapComponent tm;
    TileAnimation anim;
    anim.frames = {{1, 0.2f}, {2, 0.2f}};
    tm.AddAnimation(5, anim);

    EXPECT_NE(tm.animations.find(5), tm.animations.end());
    EXPECT_FLOAT_EQ(tm.animations[5].total_duration, 0.4f);
}

TEST(TilemapComponentTest, SetGetProperties) {
    TilemapComponent tm;
    TileProperties prop;
    prop.solid = true;
    prop.collision_type = 1;
    prop.friction = 0.8f;
    prop.Set("type", "wall");
    tm.SetProperties(3, prop);

    const auto& got = tm.GetProperties(3);
    EXPECT_TRUE(got.solid);
    EXPECT_EQ(got.collision_type, 1);
    EXPECT_FLOAT_EQ(got.friction, 0.8f);
    EXPECT_EQ(got.Get("type"), "wall");
}

TEST(TilemapComponentTest, GetPropertiesDefault) {
    TilemapComponent tm;
    const auto& got = tm.GetProperties(999);  // 不存在
    EXPECT_FALSE(got.solid);
    EXPECT_EQ(got.collision_type, 0);
}

// ── 序列化测试 ──────────────────────────────────────────────────────────────

TEST(TilemapSerializerTest, SaveLoadRoundtrip) {
    TilemapComponent tm;
    tm.width = 4;
    tm.height = 3;
    tm.tile_size = 2.0f;
    tm.tileset_cols = 8;
    tm.tileset_rows = 4;
    tm.tiles = {1, 2, 0, 3, 0, 0, 4, 5, 6, 0, 7, 8};

    // 添加层
    tm.AddLayer("background");
    auto* layer = tm.GetLayerMut(0);
    layer->opacity = 0.5f;
    layer->visible = false;
    layer->sorting_layer = 2;
    layer->order_in_layer_base = 10;
    layer->generate_colliders = true;
    layer->collider_tile_min = 2;
    for (auto& t : layer->tiles) t = 1;
    layer->tiles[0] = 5;

    // 添加动画
    TileAnimation anim;
    anim.frames = {{1, 0.1f}, {2, 0.2f}, {3, 0.15f}};
    anim.loop = false;
    tm.AddAnimation(1, anim);

    // 添加属性
    TileProperties prop;
    prop.solid = true;
    prop.collision_type = 1;
    prop.friction = 0.7f;
    prop.restitution = 0.2f;
    prop.Set("type", "wall");
    prop.Set("hp", "100");
    tm.SetProperties(1, prop);

    // 保存
    std::string tileset_path = "assets/tileset.png";
    auto data = TilemapSerializer::Save(tm, tileset_path);
    EXPECT_FALSE(data.empty());

    // 加载
    TilemapComponent loaded;
    std::string loaded_path;
    EXPECT_TRUE(TilemapSerializer::Load(data.data(), data.size(), loaded, loaded_path));

    // 验证
    EXPECT_EQ(loaded.width, 4);
    EXPECT_EQ(loaded.height, 3);
    EXPECT_FLOAT_EQ(loaded.tile_size, 2.0f);
    EXPECT_EQ(loaded.tileset_cols, 8);
    EXPECT_EQ(loaded.tileset_rows, 4);
    EXPECT_EQ(loaded_path, tileset_path);

    // 验证 tiles
    EXPECT_EQ(loaded.tiles.size(), tm.tiles.size());
    for (size_t i = 0; i < tm.tiles.size(); ++i)
        EXPECT_EQ(loaded.tiles[i], tm.tiles[i]);

    // 验证层
    EXPECT_EQ(loaded.LayerCount(), 1);
    auto* loaded_layer = loaded.GetLayer(0);
    ASSERT_NE(loaded_layer, nullptr);
    EXPECT_EQ(loaded_layer->name, "background");
    EXPECT_FLOAT_EQ(loaded_layer->opacity, 0.5f);
    EXPECT_FALSE(loaded_layer->visible);
    EXPECT_EQ(loaded_layer->sorting_layer, 2);
    EXPECT_EQ(loaded_layer->order_in_layer_base, 10);
    EXPECT_TRUE(loaded_layer->generate_colliders);
    EXPECT_EQ(loaded_layer->collider_tile_min, 2);
    EXPECT_EQ(loaded_layer->tiles[0], 5);

    // 验证动画
    ASSERT_NE(loaded.animations.find(1), loaded.animations.end());
    EXPECT_FALSE(loaded.animations[1].loop);
    EXPECT_EQ(loaded.animations[1].frames.size(), 3u);
    EXPECT_EQ(loaded.animations[1].frames[0].tile_id, 1);
    EXPECT_FLOAT_EQ(loaded.animations[1].frames[0].duration, 0.1f);
    EXPECT_FLOAT_EQ(loaded.animations[1].total_duration, 0.45f);

    // 验证属性
    ASSERT_NE(loaded.properties.find(1), loaded.properties.end());
    const auto& loaded_prop = loaded.properties[1];
    EXPECT_TRUE(loaded_prop.solid);
    EXPECT_EQ(loaded_prop.collision_type, 1);
    EXPECT_FLOAT_EQ(loaded_prop.friction, 0.7f);
    EXPECT_FLOAT_EQ(loaded_prop.restitution, 0.2f);
    EXPECT_EQ(loaded_prop.Get("type"), "wall");
    EXPECT_EQ(loaded_prop.Get("hp"), "100");
}

TEST(TilemapSerializerTest, InvalidMagic) {
    std::vector<uint8_t> bad_data = {'B', 'A', 'D', '1', 0, 0, 0, 1};
    TilemapComponent tm;
    std::string path;
    EXPECT_FALSE(TilemapSerializer::Load(bad_data.data(), bad_data.size(), tm, path));
}

TEST(TilemapSerializerTest, EmptyTilemap) {
    TilemapComponent tm;
    tm.width = 0;
    tm.height = 0;
    tm.tile_size = 1.0f;

    auto data = TilemapSerializer::Save(tm, "");
    EXPECT_FALSE(data.empty());

    TilemapComponent loaded;
    std::string path;
    EXPECT_TRUE(TilemapSerializer::Load(data.data(), data.size(), loaded, path));
    EXPECT_EQ(loaded.width, 0);
    EXPECT_EQ(loaded.height, 0);
}

TEST(TilemapSerializerTest, FileRoundtrip) {
    TilemapComponent tm;
    tm.width = 2;
    tm.height = 2;
    tm.tile_size = 1.5f;
    tm.tileset_cols = 4;
    tm.tileset_rows = 4;
    tm.tiles = {1, 2, 3, 4};

    const char* test_file = "test_tilemap.dtilemap";
    EXPECT_TRUE(TilemapSerializer::SaveToFile(test_file, tm, "test.png"));

    TilemapComponent loaded;
    std::string path;
    EXPECT_TRUE(TilemapSerializer::LoadFromFile(test_file, loaded, path));
    EXPECT_EQ(loaded.width, 2);
    EXPECT_EQ(loaded.height, 2);
    EXPECT_FLOAT_EQ(loaded.tile_size, 1.5f);
    EXPECT_EQ(path, "test.png");

    // 清理
    std::remove(test_file);
}

// ── TilemapSystem 增强测试 ──────────────────────────────────────────────────

class TilemapSystemEnhancedTest : public ::testing::Test {
protected:
    World world;
    TilemapSystem system;
};

TEST_F(TilemapSystemEnhancedTest, MultiLayerGeneratesEntities) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.width = 3;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.tileset_cols = 2;
    tilemap.tileset_rows = 2;

    // 添加两个层
    int bg = tilemap.AddLayer("background");
    int fg = tilemap.AddLayer("foreground");

    // 背景层全填充
    auto* bg_layer = tilemap.GetLayerMut(bg);
    std::fill(bg_layer->tiles.begin(), bg_layer->tiles.end(), 1);

    // 前景层部分填充
    auto* fg_layer = tilemap.GetLayerMut(fg);
    fg_layer->tiles[0] = 2;
    fg_layer->tiles[1] = 3;
    fg_layer->tiles[2] = 0;
    fg_layer->tiles[3] = 0;
    fg_layer->tiles[4] = 4;
    fg_layer->tiles[5] = 0;

    system.Update(world.registry());

    EXPECT_EQ(bg_layer->runtime_tile_entities.size(), 6u);  // 3x2 全满
    EXPECT_EQ(fg_layer->runtime_tile_entities.size(), 3u);  // 3 个非空
}

TEST_F(TilemapSystemEnhancedTest, AnimationTileSwitchesUV) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.width = 1;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tileset_cols = 4;
    tilemap.tileset_rows = 1;
    tilemap.tiles = {1};  // tile_id=1

    // 添加动画：tile_id 1 在帧0/1之间切换
    TileAnimation anim;
    anim.frames = {{1, 0.1f}, {2, 0.1f}};
    tilemap.AddAnimation(1, anim);

    // t=0: 应该使用 tile_id=1 (第一帧)
    tilemap.animation_time = 0.0f;
    tilemap.dirty = true;
    system.Update(world.registry());
    ASSERT_EQ(tilemap.runtime_tile_entities.size(), 1u);
    auto& sprite0 = world.registry().get<SpriteRendererComponent>(
        tilemap.runtime_tile_entities[0]);
    // tile_id=1 → col=0, u0=0.0
    EXPECT_FLOAT_EQ(sprite0.uv.x, 0.0f);

    // t=0.15: 应该使用 tile_id=2 (第二帧)
    tilemap.animation_time = 0.15f;
    tilemap.dirty = true;
    system.Update(world.registry());
    ASSERT_EQ(tilemap.runtime_tile_entities.size(), 1u);
    auto& sprite1 = world.registry().get<SpriteRendererComponent>(
        tilemap.runtime_tile_entities[0]);
    // tile_id=2 → col=1, u0=0.25
    EXPECT_FLOAT_EQ(sprite1.uv.x, 0.25f);
}

TEST_F(TilemapSystemEnhancedTest, LayerOpacityAppliedToSprite) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.width = 1;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tileset_cols = 1;
    tilemap.tileset_rows = 1;

    tilemap.AddLayer("transparent");
    auto* layer = tilemap.GetLayerMut(0);
    layer->tiles[0] = 1;
    layer->opacity = 0.5f;

    system.Update(world.registry());
    ASSERT_EQ(layer->runtime_tile_entities.size(), 1u);
    auto& sprite = world.registry().get<SpriteRendererComponent>(
        layer->runtime_tile_entities[0]);
    EXPECT_FLOAT_EQ(sprite.color.w, 0.5f);
}

TEST_F(TilemapSystemEnhancedTest, TilePropertiesAffectCollider) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.width = 1;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tileset_cols = 1;
    tilemap.tileset_rows = 1;
    tilemap.tiles = {1};
    tilemap.generate_colliders = true;
    tilemap.collider_tile_min = 1;

    // 设置瓦片属性：非实心，不应生成碰撞体
    TileProperties prop;
    prop.solid = false;
    prop.collision_type = 0;  // 无碰撞
    tilemap.SetProperties(1, prop);

    system.Update(world.registry());
    ASSERT_EQ(tilemap.runtime_tile_entities.size(), 1u);
    auto tile_entity = tilemap.runtime_tile_entities[0];
    // solid=false 且 collision_type=0 → 不应有碰撞体
    EXPECT_FALSE(world.registry().all_of<RigidBody2DComponent>(tile_entity));
}

TEST_F(TilemapSystemEnhancedTest, SingleLayerBackwardCompat) {
    // 旧的单层模式仍然正常工作
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1, 2, 0, 3};
    tilemap.width = 2;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;
    tilemap.tileset_cols = 2;
    tilemap.tileset_rows = 2;

    system.Update(world.registry());

    EXPECT_EQ(tilemap.runtime_tile_entities.size(), 3u);
    EXPECT_FALSE(tilemap.dirty);
}
