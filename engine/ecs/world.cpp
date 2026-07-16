/**
 * @file world.cpp
 * @brief 实体组件系统(ECS)核心，管理实体、组件生命周期和系统调度
 */

#include "engine/ecs/world.h"
#include "engine/core/service_locator.h"

Entity World::CreateEntity() {
    Entity entity = registry_.create();
    ++entity_count_;
    return entity;
}

void World::DestroyEntity(Entity entity) {
    if (registry_.valid(entity)) {
        registry_.destroy(entity);
        if (entity_count_ > 0) {
            --entity_count_;
        }
    }
}

void World::Clear() {
    // 始终清空底层 registry：不能用 entity_count_ 作为早退守卫，否则当实体被
    // 绕过 World API（例如编辑器场景加载直接操作 registry）创建时，entity_count_
    // 仍为 0 会导致这里误跳过清空，把非空 registry 的销毁推迟到进程退出期
    // （模块 DLL / RHI 已卸载之后），正是需要避免的悬挂销毁场景。
    // clear() 作用于空 registry 是廉价的空操作。
    registry_.clear();
    entity_count_ = 0;
}

World::~World() {
    Clear();
}

bool World::IsAlive(Entity entity) const {
    return registry_.valid(entity);
}

size_t World::EntityCount() const {
    return entity_count_;
}

entt::registry& World::registry() {
    return registry_;
}
