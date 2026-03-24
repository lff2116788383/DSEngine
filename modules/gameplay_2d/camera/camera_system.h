#ifndef DSE_PHASE1_CAMERA_SYSTEM_H
#define DSE_PHASE1_CAMERA_SYSTEM_H

#include "engine/ecs/world.h"

class CameraSystem {
public:
    void Update(Phase1World& world, float aspect_ratio);
};

#endif
