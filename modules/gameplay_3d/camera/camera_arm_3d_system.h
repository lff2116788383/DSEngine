#ifndef DSE_CAMERA_ARM_3D_SYSTEM_H
#define DSE_CAMERA_ARM_3D_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse::gameplay3d {

class CameraArm3DSystem {
public:
    void Update(World& world, float delta_time);
};

} // namespace dse::gameplay3d

#endif // DSE_CAMERA_ARM_3D_SYSTEM_H
