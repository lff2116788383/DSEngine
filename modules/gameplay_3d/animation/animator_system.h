#ifndef DSE_ANIMATOR_SYSTEM_H
#define DSE_ANIMATOR_SYSTEM_H

#include "engine/ecs/world.h"

namespace dse {
namespace gameplay3d {

class AnimatorSystem {
public:
    static void Update(World& world, float delta_time);
};

} // namespace gameplay3d
} // namespace dse

#endif // DSE_ANIMATOR_SYSTEM_H