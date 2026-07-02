#ifndef DSE_CHARACTER_MOVEMENT_SYSTEM_H
#define DSE_CHARACTER_MOVEMENT_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
struct CharacterMovementConfig;
struct CharacterMovementState;
struct CharacterController3DComponent;
struct SpringArm3DComponent;
}

namespace dse::gameplay3d {

class CharacterMovementSystem {
public:
    void Update(World& world, float delta_time);
};

} // namespace dse::gameplay3d

#endif // DSE_CHARACTER_MOVEMENT_SYSTEM_H
