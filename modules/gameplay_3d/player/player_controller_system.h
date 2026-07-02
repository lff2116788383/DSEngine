#ifndef DSE_PLAYER_CONTROLLER_SYSTEM_H
#define DSE_PLAYER_CONTROLLER_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse::gameplay3d {

class PlayerControllerSystem {
public:
    void Update(World& world, float delta_time);
};

} // namespace dse::gameplay3d

#endif // DSE_PLAYER_CONTROLLER_SYSTEM_H
