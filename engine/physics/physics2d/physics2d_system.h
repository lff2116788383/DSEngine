#ifndef DSE_PHYSICS2D_SYSTEM_H
#define DSE_PHYSICS2D_SYSTEM_H

#include "engine/ecs/world.h"
#include <box2d/box2d.h>
#include <memory>

// Forward declarations
class PhysicsContactListener;

class Physics2DSystem {
public:
    Physics2DSystem();
    ~Physics2DSystem();

    void Init(World& world);
    void FixedUpdate(World& world, float fixed_delta_time);
    
    // Physics Queries
    bool Raycast(const glm::vec2& start, const glm::vec2& end, Entity& out_entity, glm::vec2& out_point, glm::vec2& out_normal);

private:
    std::unique_ptr<b2World> physics_world_;
    std::unique_ptr<PhysicsContactListener> contact_listener_;
    int velocity_iterations_ = 8;
    int position_iterations_ = 3;
};

#endif
