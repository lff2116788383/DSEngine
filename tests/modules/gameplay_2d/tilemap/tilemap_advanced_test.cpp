/**
 * @file tilemap_advanced_test.cpp
 * @brief Tilemap 高级测试：大地图、动态修改、碰撞同步
 */

#include "catch/catch.hpp"
#include "modules/gameplay_2d/tilemap/tilemap_system.h"
#include "engine/ecs/world.h"
#include "engine/ecs/components_2d.h"

using dse::gameplay2d::TilemapSystem;

// ─── Large map ──────────────────────────────────────────────────────────
TEST_CASE("Tilemap Advanced - large map generates correct tile count", "[tilemap][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);

    const int w = 100;
    const int h = 100;
    tilemap.width = w;
    tilemap.height = h;
    tilemap.tile_size = 1.0f;
    tilemap.tiles.resize(w * h, 0);

    // Fill every other tile
    int expected_count = 0;
    for (int i = 0; i < w * h; ++i) {
        if (i % 2 == 0) {
            tilemap.tiles[i] = 1;
            ++expected_count;
        }
    }
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == static_cast<size_t>(expected_count));
    REQUIRE_FALSE(tilemap.dirty);
}

// ─── Dynamic tile modification ──────────────────────────────────────────
TEST_CASE("Tilemap Advanced - modifying tiles and re-dirtying rebuilds correctly", "[tilemap][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    tilemap.width = 3;
    tilemap.height = 3;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {1, 1, 1, 0, 0, 0, 1, 1, 1};
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());
    REQUIRE(tilemap.runtime_tile_entities.size() == 6);  // 6 non-zero tiles

    // Modify: clear top row
    tilemap.tiles[0] = 0;
    tilemap.tiles[1] = 0;
    tilemap.tiles[2] = 0;
    tilemap.dirty = true;
    system.Update(world.registry());
    REQUIRE(tilemap.runtime_tile_entities.size() == 3);  // only bottom row
}

// ─── Tile size affects entity positions ─────────────────────────────────
TEST_CASE("Tilemap Advanced - tile size affects runtime entity positions", "[tilemap][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    auto& tf = world.registry().emplace<TransformComponent>(entity);
    tf.position = glm::vec3(0.0f, 0.0f, 0.0f);
    tilemap.width = 2;
    tilemap.height = 1;
    tilemap.tile_size = 2.0f;  // larger tiles
    tilemap.tiles = {1, 2};
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.size() == 2);

    // Check that tiles are spaced by tile_size
    if (tilemap.runtime_tile_entities.size() >= 2) {
        auto e0 = tilemap.runtime_tile_entities[0];
        auto e1 = tilemap.runtime_tile_entities[1];
        if (world.registry().valid(e0) && world.registry().valid(e1)) {
            auto& tf0 = world.registry().get<TransformComponent>(e0);
            auto& tf1 = world.registry().get<TransformComponent>(e1);
            float dx = std::abs(tf1.position.x - tf0.position.x);
            REQUIRE(dx == Approx(2.0f).margin(0.1f));
        }
    }
}

// ─── Collider generation with different tile_min ────────────────────────
TEST_CASE("Tilemap Advanced - collider_tile_min filters which tiles get colliders", "[tilemap][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    tilemap.width = 4;
    tilemap.height = 1;
    tilemap.tile_size = 1.0f;
    tilemap.tiles = {0, 1, 2, 3};
    tilemap.generate_colliders = true;
    tilemap.collider_tile_min = 2;  // only tiles >= 2 get colliders
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    // Count entities with colliders
    int collider_count = 0;
    for (auto re : tilemap.runtime_tile_entities) {
        if (world.registry().valid(re) && world.registry().all_of<BoxCollider2DComponent>(re)) {
            ++collider_count;
        }
    }
    REQUIRE(collider_count == 2);  // tiles 2 and 3
}

// ─── Empty map ──────────────────────────────────────────────────────────
TEST_CASE("Tilemap Advanced - all-zero map creates no runtime entities", "[tilemap][advanced]") {
    World world;
    auto entity = world.CreateEntity();
    auto& tilemap = world.registry().emplace<TilemapComponent>(entity);
    world.registry().emplace<TransformComponent>(entity);
    tilemap.width = 5;
    tilemap.height = 5;
    tilemap.tile_size = 1.0f;
    tilemap.tiles.resize(25, 0);
    tilemap.dirty = true;

    TilemapSystem system;
    system.Update(world.registry());

    REQUIRE(tilemap.runtime_tile_entities.empty());
}
