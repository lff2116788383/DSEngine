/**
 * @file tilemap_system.cpp
 * @brief 瓦片地图系统 — 增强版（多层、动画瓦片、瓦片属性）
 *
 * 增强：
 * - 多层渲染：遍历 layers 数组，每层独立生成运行时实体
 * - 动画瓦片：每帧更新 animation_time，切换动画瓦片的 UV
 * - 向后兼容：layers 为空时回退到单层模式
 */

#include "tilemap_system.h"
#include "engine/ecs/components_2d.h"
#include <glm/glm.hpp>

namespace dse {
namespace gameplay2d {

// ── 正式实现 ──────────────────────────────────────────────────────────────

void TilemapSystem::Update(entt::registry& registry) {
    auto view = registry.view<TilemapComponent, TransformComponent>();
    for (auto entity : view) {
        auto& tilemap = view.get<TilemapComponent>(entity);
        auto& transform = view.get<TransformComponent>(entity);

        // 数据校验
        if (tilemap.width <= 0 || tilemap.height <= 0) continue;

        // 更新动画时间
        // animation_time 由外部 Tick 更新（这里只读）
        // 如果有动画瓦片，标记 dirty
        bool has_animations = !tilemap.animations.empty();

        // transform dirty 传播
        if (transform.dirty) {
            tilemap.dirty = true;
            for (auto& layer : tilemap.layers) layer.dirty = true;
        }

        // ── 多层模式 ──
        if (!tilemap.layers.empty()) {
            for (auto& layer : tilemap.layers) {
                if (!layer.dirty && !has_animations) continue;

                // 清理旧实体
                for (auto e : layer.runtime_tile_entities) {
                    if (registry.valid(e)) registry.destroy(e);
                }
                layer.runtime_tile_entities.clear();

                if (layer.tiles.size() != static_cast<size_t>(layer.width * layer.height))
                    continue;
                if (layer.width <= 0 || layer.height <= 0) {
                    layer.dirty = false;
                    continue;
                }

                const float ts = tilemap.tile_size;
                const float half_tile = ts * 0.5f;
                const float map_w = static_cast<float>(tilemap.width) * ts;
                const float map_h = static_cast<float>(tilemap.height) * ts;
                const float origin_x = transform.position.x - map_w * 0.5f + half_tile;
                const float origin_y = transform.position.y - map_h * 0.5f + half_tile;

                for (int y = 0; y < layer.height; ++y) {
                    for (int x = 0; x < layer.width; ++x) {
                        const int idx = y * layer.width + x;
                        int tile_id = layer.tiles[idx];
                        if (tile_id <= 0) continue;

                        // 动画瓦片帧切换
                        auto anim_it = tilemap.animations.find(tile_id);
                        if (anim_it != tilemap.animations.end()) {
                            tile_id = anim_it->second.GetFrameAt(tilemap.animation_time);
                            if (tile_id <= 0) tile_id = 1;
                        }

                        // 创建实体
                        const Entity tile_entity = registry.create();
                        auto& tile_tf = registry.emplace<TransformComponent>(tile_entity);
                        tile_tf.position = glm::vec3(
                            origin_x + static_cast<float>(x) * ts,
                            origin_y + static_cast<float>(y) * ts,
                            transform.position.z
                        );
                        tile_tf.rotation = transform.rotation;
                        tile_tf.scale = glm::vec3(ts, ts, 1.0f);
                        tile_tf.dirty = true;

                        auto& sprite = registry.emplace<SpriteRendererComponent>(tile_entity);
                        sprite.texture = tilemap.tileset_texture;
                        sprite.texture_handle = tilemap.tileset_handle;
                        sprite.sorting_layer = layer.sorting_layer;
                        sprite.order_in_layer = layer.order_in_layer_base + idx;
                        sprite.visible = layer.visible;
                        if (layer.opacity < 1.0f)
                            sprite.color = glm::vec4(1.0f, 1.0f, 1.0f, layer.opacity);

                        // UV
                        int safe_cols = tilemap.tileset_cols > 0 ? tilemap.tileset_cols : 1;
                        int safe_rows = tilemap.tileset_rows > 0 ? tilemap.tileset_rows : 1;
                        int tile_index = tile_id - 1;
                        int col = tile_index % safe_cols;
                        int row = tile_index / safe_cols;
                        if (row >= safe_rows) row = safe_rows - 1;
                        float inv_cols = 1.0f / static_cast<float>(safe_cols);
                        float inv_rows = 1.0f / static_cast<float>(safe_rows);
                        float u0 = static_cast<float>(col) * inv_cols;
                        float v0 = static_cast<float>(row) * inv_rows;
                        sprite.uv = glm::vec4(u0, v0, u0 + inv_cols, v0 + inv_rows);

                        // 碰撞体
                        if (layer.generate_colliders && tile_id >= layer.collider_tile_min) {
                            auto prop_it = tilemap.properties.find(tile_id);
                            bool is_solid = true;
                            float friction = 0.4f;
                            float restitution = 0.0f;
                            if (prop_it != tilemap.properties.end()) {
                                is_solid = prop_it->second.solid ||
                                          prop_it->second.collision_type > 0;
                                friction = prop_it->second.friction;
                                restitution = prop_it->second.restitution;
                            }
                            if (is_solid) {
                                auto& rb = registry.emplace<RigidBody2DComponent>(tile_entity);
                                rb.type = RigidBody2DType::Static;
                                rb.gravity_scale = 0.0f;
                                rb.fixed_rotation = true;

                                auto& collider = registry.emplace<BoxCollider2DComponent>(tile_entity);
                                collider.size = glm::vec2(ts, ts);
                                collider.density = 1.0f;
                                collider.friction = friction;
                                collider.restitution = restitution;
                            }
                        }

                        layer.runtime_tile_entities.push_back(tile_entity);
                    }
                }

                layer.dirty = false;
            }

            // 动画帧更新：如果有动画，每帧都需要标记 dirty 重新生成
            if (has_animations) {
                for (auto& layer : tilemap.layers) layer.dirty = true;
            }
        } else {
            // ── 单层模式（向后兼容） ──
            if (!tilemap.dirty && !has_animations) continue;

            // 数据校验
            if (tilemap.tiles.size() != static_cast<size_t>(tilemap.width * tilemap.height))
                continue;

            // 清理旧实体
            for (auto e : tilemap.runtime_tile_entities) {
                if (registry.valid(e)) registry.destroy(e);
            }
            tilemap.runtime_tile_entities.clear();

            const float ts = tilemap.tile_size;
            const float half_tile = ts * 0.5f;
            const float map_w = static_cast<float>(tilemap.width) * ts;
            const float map_h = static_cast<float>(tilemap.height) * ts;
            const float origin_x = transform.position.x - map_w * 0.5f + half_tile;
            const float origin_y = transform.position.y - map_h * 0.5f + half_tile;

            for (int y = 0; y < tilemap.height; ++y) {
                for (int x = 0; x < tilemap.width; ++x) {
                    const int idx = y * tilemap.width + x;
                    int tile_id = tilemap.tiles[idx];
                    if (tile_id <= 0) continue;

                    // 动画帧切换
                    auto anim_it = tilemap.animations.find(tile_id);
                    if (anim_it != tilemap.animations.end()) {
                        tile_id = anim_it->second.GetFrameAt(tilemap.animation_time);
                        if (tile_id <= 0) tile_id = 1;
                    }

                    // 创建实体
                    const Entity tile_entity = registry.create();
                    auto& tile_tf = registry.emplace<TransformComponent>(tile_entity);
                    tile_tf.position = glm::vec3(
                        origin_x + static_cast<float>(x) * ts,
                        origin_y + static_cast<float>(y) * ts,
                        transform.position.z
                    );
                    tile_tf.rotation = transform.rotation;
                    tile_tf.scale = glm::vec3(ts, ts, 1.0f);
                    tile_tf.dirty = true;

                    auto& sprite = registry.emplace<SpriteRendererComponent>(tile_entity);
                    sprite.texture = tilemap.tileset_texture;
                    sprite.texture_handle = tilemap.tileset_handle;
                    sprite.sorting_layer = tilemap.sorting_layer;
                    sprite.order_in_layer = tilemap.order_in_layer_base + idx;
                    sprite.visible = true;

                    // UV
                    int safe_cols = tilemap.tileset_cols > 0 ? tilemap.tileset_cols : 1;
                    int safe_rows = tilemap.tileset_rows > 0 ? tilemap.tileset_rows : 1;
                    int tile_index = tile_id - 1;
                    int col = tile_index % safe_cols;
                    int row = tile_index / safe_cols;
                    if (row >= safe_rows) row = safe_rows - 1;
                    float inv_cols = 1.0f / static_cast<float>(safe_cols);
                    float inv_rows = 1.0f / static_cast<float>(safe_rows);
                    float u0 = static_cast<float>(col) * inv_cols;
                    float v0 = static_cast<float>(row) * inv_rows;
                    sprite.uv = glm::vec4(u0, v0, u0 + inv_cols, v0 + inv_rows);

                    // 碰撞体
                    if (tilemap.generate_colliders && tile_id >= tilemap.collider_tile_min) {
                        auto prop_it = tilemap.properties.find(tile_id);
                        bool is_solid = true;
                        float friction = 0.4f;
                        float restitution = 0.0f;
                        if (prop_it != tilemap.properties.end()) {
                            is_solid = prop_it->second.solid ||
                                      prop_it->second.collision_type > 0;
                            friction = prop_it->second.friction;
                            restitution = prop_it->second.restitution;
                        }
                        if (is_solid) {
                            auto& rb = registry.emplace<RigidBody2DComponent>(tile_entity);
                            rb.type = RigidBody2DType::Static;
                            rb.gravity_scale = 0.0f;
                            rb.fixed_rotation = true;

                            auto& collider = registry.emplace<BoxCollider2DComponent>(tile_entity);
                            collider.size = glm::vec2(ts, ts);
                            collider.density = 1.0f;
                            collider.friction = friction;
                            collider.restitution = restitution;
                        }
                    }

                    tilemap.runtime_tile_entities.push_back(tile_entity);
                }
            }

            tilemap.dirty = false;

            // 动画帧更新：如果有动画，每帧都标记 dirty
            if (has_animations) {
                tilemap.dirty = true;
            }
        }
    }
}

} // namespace gameplay2d
} // namespace dse
