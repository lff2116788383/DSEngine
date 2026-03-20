#include "phase1/ecs/world.h"

Phase1World& Phase1World::Instance() {
    static Phase1World instance;
    return instance;
}

Entity Phase1World::CreateEntity() {
    return registry_.create();
}

void Phase1World::DestroyEntity(Entity entity) {
    registry_.destroy(entity);
}

void Phase1World::Clear() {
    registry_.clear();
}

bool Phase1World::IsAlive(Entity entity) const {
    return registry_.valid(entity);
}

entt::registry& Phase1World::registry() {
    return registry_;
}
