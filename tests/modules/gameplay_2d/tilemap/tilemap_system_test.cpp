#include "catch/catch.hpp"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

using dse::gameplay2d::TilemapSystem;

// 正向测试：合法瓦片地图更新后应创建运行时瓦片实体并清除 dirty 标记。
TEST_CASE("Given_ValidTilemap_When_Update_Then_RuntimeTilesAreGenerated", "[engine][unit][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {1, -1};
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 1);
    REQUIRE_FALSE(tilemap.dirty);
}

// 边界测试：无效尺寸或 tile_size 时应安全退出且不创建运行时实体。
TEST_CASE("Given_InvalidMapSize_When_Update_Then_RuntimeTilesRemainEmpty", "[engine][unit][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    tilemap.width = 0;
    tilemap.height = 0;
    tilemap.tile_size = 0.0f;
    tilemap.tiles.clear();
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.empty());
    REQUIRE_FALSE(tilemap.dirty);
}

// 反向测试：tiles 数据长度与宽高不一致时，应直接跳过本次处理。
TEST_CASE("Given_MismatchedTileData_When_Update_Then_SystemSkipsGeneration", "[engine][unit][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    tilemap.width = 2;
    tilemap.height = 2;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {1, 2, 3};
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.empty());
    REQUIRE(tilemap.dirty);
}

// 回归测试：开启 generate_colliders 时，符合条件的瓦片应生成静态刚体与碰撞体。
TEST_CASE("Given_ColliderTilemap_When_Update_Then_RuntimeTilesContainStaticColliders", "[engine][unit][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {0, 2};
    tilemap.generate_colliders = true;
    tilemap.collider_tile_min = 1;
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 2);

    const auto collider_entity = tilemap.runtime_tile_entities[1];
    REQUIRE(world.registry().all_of<RigidBody2DComponent>(collider_entity));
    REQUIRE(world.registry().all_of<BoxCollider2DComponent>(collider_entity));
    const auto& rb = world.registry().get<RigidBody2DComponent>(collider_entity);
    REQUIRE(rb.type == RigidBody2DType::Static);
}

// 回归测试：dirty 重建时应销毁旧运行时瓦片，并按最新 tiles 重新生成。
TEST_CASE("Given_DirtyTilemapRefresh_When_UpdateAgain_Then_RuntimeTilesAreRebuilt", "[engine][unit][tilemap]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    tf.dirty = false;
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {1, -1};
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 1);
    const auto first_runtime = tilemap.runtime_tile_entities.front();
    REQUIRE(world.registry().valid(first_runtime));

    tilemap.tiles = {-1, 3};
    tilemap.dirty = true;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 1);
    REQUIRE_FALSE(world.registry().valid(first_runtime));
    REQUIRE(world.registry().valid(tilemap.runtime_tile_entities.front()));
}
