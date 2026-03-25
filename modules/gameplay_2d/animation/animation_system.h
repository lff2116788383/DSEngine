#ifndef DSE_ANIMATION_SYSTEM_H
#define DSE_ANIMATION_SYSTEM_H

#include "engine/ecs/world.h"

class AnimationSystem {
public:
    void Update(World& world, float delta_time);
};

#endif
