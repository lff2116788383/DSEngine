/**
 * @file world.h
 * @brief 实体组件系统(ECS)核心，管理实体、组件生命周期和系统调度
 */

#ifndef DSE_WORLD_H
#define DSE_WORLD_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <entt/entt.hpp>
#include "engine/ecs/components_2d.h"
#include "engine/ecs/components_3d.h"

using Entity = entt::entity;

/**
 * @class World
 * @brief 实体世界管理器，封装 EnTT registry，提供实体的创建、销毁和查询接口。
 */
class World {
public:
    /**
     * @brief 获取 World 单例
     * @return World 实例引用
     */
    static World& Instance();

    /**
     * @brief 创建一个新的空实体
     * @return 新创建的实体 ID
     * @example
     * // Entity e = World::Instance().CreateEntity();
     */
    Entity CreateEntity();
    
    /**
     * @brief 销毁指定的实体及其所有组件
     * @param entity 要销毁的实体 ID
     */
    void DestroyEntity(Entity entity);
    
    /**
     * @brief 清空世界中的所有实体和组件
     */
    void Clear();

    ~World();

    /**
     * @brief 检查实体是否存活（未被销毁）
     * @param entity 实体 ID
     * @return 存活返回 true，否则返回 false
     */
    bool IsAlive(Entity entity) const;
    
    /**
     * @brief 获取当前世界中存活的实体总数
     * @return 实体数量
     */
    size_t EntityCount() const;

    /**
     * @brief 获取底层的 EnTT registry 引用，用于进行组件查询和遍历
     * @return entt::registry 引用
     */
    entt::registry& registry();

private:
    entt::registry registry_;
    size_t entity_count_ = 0;
};

#endif
