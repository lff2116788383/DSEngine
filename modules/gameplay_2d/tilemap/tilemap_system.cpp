/**
 * @file tilemap_system.cpp
 * @brief 瓦片地图系统，处理 2D 网格地图的加载、解析和高效渲染
 */

#include "tilemap_system.h"
#include "engine/ecs/components_2d.h"
#include <glm/glm.hpp>

namespace dse {
namespace gameplay2d {

void TilemapSystem::Update(entt::registry& registry) {
    auto view = registry.view<TilemapComponent, TransformComponent>();
    for (auto entity : view) {
        auto& tilemap = view.get<TilemapComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        if (tilemap.tiles.size() != static_cast<size_t>(tilemap.width * tilemap.height)) {
            // Invalid tilemap data
            continue;
        }

        if (transform.dirty) {
            tilemap.dirty = true;
        }
        if (!tilemap.dirty) {
            continue;
        }

        for (auto runtime_entity : tilemap.runtime_tile_entities) {
            if (registry.valid(runtime_entity)) {
                registry.destroy(runtime_entity);
            }
        }
        tilemap.runtime_tile_entities.clear();

        if (tilemap.width <= 0 || tilemap.height <= 0 || tilemap.tile_size <= 0.0f) {
            tilemap.dirty = false;
            continue;
        }

        const float half_tile = tilemap.tile_size * 0.5f;
        const float map_width = static_cast<float>(tilemap.width) * tilemap.tile_size;
        const float map_height = static_cast<float>(tilemap.height) * tilemap.tile_size;
        const float origin_x = transform.position.x - map_width * 0.5f + half_tile;
        const float origin_y = transform.position.y - map_height * 0.5f + half_tile;

        for (int y = 0; y < tilemap.height; ++y) {
            for (int x = 0; x < tilemap.width; ++x) {
                const int index = y * tilemap.width + x;
                const int tile_id = tilemap.tiles[index];
                if (tile_id <= 0) {
                    continue;
                }

                const Entity tile_entity = registry.create();
                auto& tile_transform = registry.emplace<TransformComponent>(tile_entity);
                tile_transform.position = glm::vec3(
                    origin_x + static_cast<float>(x) * tilemap.tile_size,
                    origin_y + static_cast<float>(y) * tilemap.tile_size,
                    transform.position.z
                );
                tile_transform.rotation = transform.rotation;
                tile_transform.scale = glm::vec3(tilemap.tile_size, tilemap.tile_size, 1.0f);
                tile_transform.dirty = true;

                auto& sprite = registry.emplace<SpriteRendererComponent>(tile_entity);
                sprite.texture = tilemap.tileset_texture;
                sprite.texture_handle = tilemap.tileset_handle;
                sprite.sorting_layer = tilemap.sorting_layer;
                sprite.order_in_layer = tilemap.order_in_layer_base + index;
                sprite.visible = true;

                int safe_cols = tilemap.tileset_cols > 0 ? tilemap.tileset_cols : 1;
                int safe_rows = tilemap.tileset_rows > 0 ? tilemap.tileset_rows : 1;
                int tile_index = tile_id - 1;
                int col = tile_index % safe_cols;
                int row = tile_index / safe_cols;
                if (row >= safe_rows) {
                    row = safe_rows - 1;
                }
                float inv_cols = 1.0f / static_cast<float>(safe_cols);
                float inv_rows = 1.0f / static_cast<float>(safe_rows);
                float u0 = static_cast<float>(col) * inv_cols;
                float v0 = static_cast<float>(row) * inv_rows;
                float u1 = u0 + inv_cols;
                float v1 = v0 + inv_rows;
                sprite.uv = glm::vec4(u0, v0, u1, v1);

                if (tilemap.generate_colliders && tile_id >= tilemap.collider_tile_min) {
                    auto& rb = registry.emplace<RigidBody2DComponent>(tile_entity);
                    rb.type = RigidBody2DType::Static;
                    rb.gravity_scale = 0.0f;
                    rb.fixed_rotation = true;

                    auto& collider = registry.emplace<BoxCollider2DComponent>(tile_entity);
                    collider.size = glm::vec2(tilemap.tile_size, tilemap.tile_size);
                    collider.density = 1.0f;
                    collider.friction = 0.4f;
                    collider.restitution = 0.0f;
                }

                tilemap.runtime_tile_entities.push_back(tile_entity);
            }
        }

        tilemap.dirty = false;
    }
}

} // namespace gameplay2d
} // namespace dse
