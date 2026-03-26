/**
 * @file world.cpp
 * @brief 实体组件系统(ECS)核心，管理实体、组件生命周期和系统调度
 */

#include "engine/ecs/world.h"

World& World::Instance() {
    static World instance;
    return instance;
}

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
    registry_.clear();
    entity_count_ = 0;
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
