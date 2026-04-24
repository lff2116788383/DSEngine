/**
 * @file world.h
 * @brief 实体组件系统(ECS)核心，管理实体、组件生命周期和系统调度
 *
 * 改进点：
 * - World 不再强制单例，改为可实例化
 * - Instance() 保留兼容，委托到 ServiceLocator
 * - 支持多 World 并行运行
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
 *
 * 生命周期管理：
 * - 推荐通过 EngineInstance 或 ServiceLocator 获取 World 实例
 * - Instance() 保留作为兼容过渡
 * - 支持创建多个独立 World（多场景并行）
 *
 * @example
 * // 新用法（推荐）- 通过 ServiceLocator
 * auto* world = ServiceLocator::Instance().Get<World>();
 *
 * // 旧用法（兼容）
 * World::Instance().CreateEntity();
 */
class World {
public:
    World() = default;

    /**
     * @brief 获取 World 默认实例（兼容过渡，委托到 ServiceLocator）
     * @return World 实例引用
     * @deprecated 通过 EngineInstance 或 ServiceLocator 获取 World
     */
    static World& Instance();
    
    /**
     * @brief 创建一个新的空实体
     * @return 新创建的实体 ID
     * @example
     * Entity e = world.CreateEntity();
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
