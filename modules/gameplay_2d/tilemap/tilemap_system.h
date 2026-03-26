/**
 * @file tilemap_system.h
 * @brief 瓦片地图系统，处理 2D 网格地图的加载、解析和高效渲染
 */

#ifndef DSE_TILEMAP_SYSTEM_H
#define DSE_TILEMAP_SYSTEM_H

#include <entt/entt.hpp>

namespace dse {
namespace gameplay2d {

/**
 * @class TilemapSystem
 * @brief 瓦片地图系统，负责更新 Tilemap 组件，生成碰撞体并准备渲染数据
 */
class TilemapSystem {
public:
    TilemapSystem() = default;
    ~TilemapSystem() = default;

    /**
     * @brief 每帧更新瓦片地图，处理动态修改和物理同步
     * @param registry ECS注册表，用于查询包含 TilemapComponent 的实体
     * @example
     * // tilemap_system.Update(world.registry());
     */
    void Update(entt::registry& registry);
};

} // namespace gameplay2d
} // namespace dse

#endif
