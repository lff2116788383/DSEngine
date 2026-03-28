#ifndef DSE_FREE_CAMERA_CONTROLLER_SYSTEM_H
#define DSE_FREE_CAMERA_CONTROLLER_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
namespace gameplay3d {

class FreeCameraControllerSystem {
public:
    void Update(World& world, float delta_time);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_FREE_CAMERA_CONTROLLER_SYSTEM_H
