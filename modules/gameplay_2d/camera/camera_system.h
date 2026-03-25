#ifndef DSE_CAMERA_SYSTEM_H
#define DSE_CAMERA_SYSTEM_H

#include "engine/ecs/world.h"

class CameraSystem {
public:
    void Update(World& world, float aspect_ratio);
};

#endif
