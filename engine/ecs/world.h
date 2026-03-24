#ifndef DSE_PHASE1_WORLD_H
#define DSE_PHASE1_WORLD_H

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <entt/entt.hpp>
#include "engine/ecs/components_2d.h"

using Entity = entt::entity;

class Phase1World {
public:
    static Phase1World& Instance();

    Entity CreateEntity();
    void DestroyEntity(Entity entity);
    void Clear();

    bool IsAlive(Entity entity) const;
    size_t EntityCount() const;

    entt::registry& registry();

private:
    entt::registry registry_;
    size_t entity_count_ = 0;
};

#endif
