/**
 * @file tilemap_system_test.cpp
 * @brief 瓦片地图系统单元测试
 *
 * 覆盖场景：
 * - 空 World 调用 Update 不崩溃
 * - 无 TilemapComponent 时 Update 不崩溃
 * - 有效瓦片数据生成运行时实体
 * - 空瓦片ID(tile_id=0)不生成实体
 * - 无效瓦片数据(size != width*height)跳过
 * - dirty 标志控制更新
 * - Transform dirty 传播至 tilemap dirty
 * - generate_colliders 生成碰撞体
 * - 瓦片 UV 根据图集行列计算
 */

#include <gtest/gtest.h>
#include "engine/ecs/world.h"
#include "engine/ecs/tilemap.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_2d.h"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"

using namespace dse;
using namespace gameplay2d;

class TilemapSystemTest : public ::testing::Test {
protected:
    World world;
    TilemapSystem system;
};

TEST_F(TilemapSystemTest, EmptyWorldCallsUpdateDoesNotCrash) {
    EXPECT_NO_THROW(system.Update(world.registry()));
}

TEST_F(TilemapSystemTest, WithoutTilemapComponentWhenUpdateDoesNotCrash) {
    auto entity = world.CreateEntity();
    world.registry().emplace<TransformComponent>(entity);
    EXPECT_NO_THROW(system.Update(world.registry()));
}

TEST_F(TilemapSystemTest, ValiddatagenerateWhenEntity) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1, 2, 0, 3}; // 2x2 网格，一个空格
    tilemap.width = 2;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;
    tilemap.tileset_cols = 2;
    tilemap.tileset_rows = 2;

    system.Update(world.registry());

    // 3 个非空瓦片应生成 3 个运行时实体
    EXPECT_EQ(tilemap.runtime_tile_entities.size(), 3u);
    EXPECT_FALSE(tilemap.dirty);
}

TEST_F(TilemapSystemTest, EmptyIDNotgenerateEntity) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {0, 0, 0, 0}; // 全空
    tilemap.width = 2;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;

    system.Update(world.registry());

    EXPECT_EQ(tilemap.runtime_tile_entities.size(), 0u);
}

TEST_F(TilemapSystemTest, Invaliddata) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1, 2}; // size=2 但 width*height=4
    tilemap.width = 2;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;

    system.Update(world.registry());

    // 数据不匹配，不应生成实体
    EXPECT_EQ(tilemap.runtime_tile_entities.size(), 0u);
}

TEST_F(TilemapSystemTest, DirtyIsfalseNotUpdatedFromTimeToTime) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.dirty = false; // Transform 也不脏
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1, 2};
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = false; // 不需要更新

    system.Update(world.registry());

    EXPECT_EQ(tilemap.runtime_tile_entities.size(), 0u);
}

TEST_F(TilemapSystemTest, TransformDirtyspreadToTilemap) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(5.0f, 0.0f, 0.0f);
    tf.dirty = true;
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1};
    tilemap.width = 1;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = false; // tilemap 本身不脏，但 transform 脏

    system.Update(world.registry());

    // transform dirty 导致 tilemap 被标记为 dirty 并更新
    EXPECT_EQ(tilemap.runtime_tile_entities.size(), 1u);
    EXPECT_FALSE(tilemap.dirty);
}

TEST_F(TilemapSystemTest, generate_CollidersGenerateCollisionBody) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1};
    tilemap.width = 1;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;
    tilemap.generate_colliders = true;
    tilemap.collider_tile_min = 1;

    system.Update(world.registry());

    ASSERT_EQ(tilemap.runtime_tile_entities.size(), 1u);
    auto tile_entity = tilemap.runtime_tile_entities[0];
    EXPECT_TRUE(world.registry().all_of<RigidBody2DComponent>(tile_entity));
    EXPECT_TRUE(world.registry().all_of<BoxCollider2DComponent>(tile_entity));
}

TEST_F(TilemapSystemTest, UVset) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1, 2, 3, 4}; // 2x2 网格
    tilemap.width = 2;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;
    tilemap.tileset_cols = 2;
    tilemap.tileset_rows = 2;

    system.Update(world.registry());

    ASSERT_EQ(tilemap.runtime_tile_entities.size(), 4u);
    // 验证第一个瓦片的 UV（tile_id=1, col=0, row=0）
    auto& sprite0 = world.registry().get<SpriteRendererComponent>(tilemap.runtime_tile_entities[0]);
    EXPECT_FLOAT_EQ(sprite0.uv.x, 0.0f);  // u0
    EXPECT_FLOAT_EQ(sprite0.uv.y, 0.0f);  // v0
    EXPECT_FLOAT_EQ(sprite0.uv.z, 0.5f);  // u1
    EXPECT_FLOAT_EQ(sprite0.uv.w, 0.5f);  // v1
}

TEST_F(TilemapSystemTest, UpdateNotgenerateEntity) {
    auto entity = world.CreateEntity();
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    tilemap.tiles = {1, 2};
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.dirty = true;

    system.Update(world.registry());
    size_t first_count = tilemap.runtime_tile_entities.size();
    EXPECT_EQ(first_count, 2u);

    // 第二次更新（dirty 已被清除）
    system.Update(world.registry());
    EXPECT_EQ(tilemap.runtime_tile_entities.size(), first_count);
}
