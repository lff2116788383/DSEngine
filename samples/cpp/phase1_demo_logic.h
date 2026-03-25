#ifndef DSE_EXAMPLES_CPP_PHASE1_DEMO_LOGIC_H
#define DSE_EXAMPLES_CPP_PHASE1_DEMO_LOGIC_H

#include "engine/ecs/world.h"

namespace dse::samples::cpp_demo {
void Bootstrap(World& world);
void Tick(World& world, float delta_time);
void Shutdown();
}

#endif
