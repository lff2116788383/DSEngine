#include "engine/ecs/world.h"

Phase1World& Phase1World::Instance() {
    static Phase1World instance;
    return instance;
}

Entity Phase1World::CreateEntity() {
    Entity entity = registry_.create();
    ++entity_count_;
    return entity;
}

void Phase1World::DestroyEntity(Entity entity) {
    if (registry_.valid(entity)) {
        registry_.destroy(entity);
        if (entity_count_ > 0) {
            --entity_count_;
        }
    }
}

void Phase1World::Clear() {
    registry_.clear();
    entity_count_ = 0;
}

bool Phase1World::IsAlive(Entity entity) const {
    return registry_.valid(entity);
}

size_t Phase1World::EntityCount() const {
    return entity_count_;
}

entt::registry& Phase1World::registry() {
    return registry_;
}
