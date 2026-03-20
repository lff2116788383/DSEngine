#ifndef DSE_PHASE1_PHYSICS2D_SYSTEM_H
#define DSE_PHASE1_PHYSICS2D_SYSTEM_H

#include "phase1/ecs/world.h"
#include <box2d/box2d.h>
#include <memory>

class Physics2DSystem {
public:
    Physics2DSystem();
    ~Physics2DSystem();

    void Init();
    void FixedUpdate(Phase1World& world, float fixed_delta_time);

private:
    std::unique_ptr<b2World> physics_world_;
    int velocity_iterations_ = 8;
    int position_iterations_ = 3;
};

#endif
