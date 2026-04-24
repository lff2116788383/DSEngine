/**
 * @file world.cpp
 * @brief 实体组件系统(ECS)核心，管理实体、组件生命周期和系统调度
 */

#include "engine/ecs/world.h"
#include "engine/core/service_locator.h"

World& World::Instance() {
    // 委托到 ServiceLocator，若未注册则自动创建并注册
    auto& locator = dse::core::ServiceLocator::Instance();
    auto* existing = locator.Get<World>();
    if (!existing) {
        auto instance = std::make_shared<World>();
        locator.Register<World, World>(instance);
        return *instance;
    }
    return *existing;
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
