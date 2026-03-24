#ifndef DSE_PHASE1_ANIMATION_SYSTEM_H
#define DSE_PHASE1_ANIMATION_SYSTEM_H

#include "engine/ecs/world.h"

class AnimationSystem {
public:
    void Update(Phase1World& world, float delta_time);
};

#endif // DSE_PHASE1_ANIMATION_SYSTEM_H
