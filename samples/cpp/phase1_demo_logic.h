#ifndef DSE_EXAMPLES_CPP_PHASE1_DEMO_LOGIC_H
#define DSE_EXAMPLES_CPP_PHASE1_DEMO_LOGIC_H

#include "engine/ecs/world.h"

namespace phase1::samples::cpp_demo {
void Bootstrap(Phase1World& world);
void Tick(Phase1World& world, float delta_time);
void Shutdown();
}

#endif
