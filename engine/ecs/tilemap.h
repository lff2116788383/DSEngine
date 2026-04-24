/**
 * @file tilemap.h
 * @brief 瓦片地图组件
 */

#ifndef DSE_ECS_COMPONENTS_2D_TILEMAP_H
#define DSE_ECS_COMPONENTS_2D_TILEMAP_H

#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <entt/entt.hpp>

class TextureAsset;
using Entity = entt::entity;

/**
 * @struct TilemapComponent
 * @brief 瓦片地图组件，管理网格地图数据和渲染图集
 */
struct TilemapComponent {
    std::vector<int> tiles;                              ///< 一维数组存储的瓦片 ID (0 为空)
    int width = 0;                                       ///< 地图的列数
    int height = 0;                                      ///< 地图的行数
    float tile_size = 1.0f;                              ///< 单个瓦片的物理/渲染尺寸
    std::shared_ptr<TextureAsset> tileset_texture;       ///< 引用的瓦片图集纹理
    unsigned int tileset_handle = 0;                     ///< 图集的 RHI 渲染句柄
    int tileset_cols = 1;
    int tileset_rows = 1;
    int sorting_layer = 0;
    int order_in_layer_base = 0;
    bool generate_colliders = false;
    int collider_tile_min = 1;
    bool dirty = true;
    std::vector<Entity> runtime_tile_entities;
};

#endif // DSE_ECS_COMPONENTS_2D_TILEMAP_H
