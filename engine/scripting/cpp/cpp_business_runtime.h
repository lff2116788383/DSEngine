#ifndef DSE_PHASE1_CPP_BUSINESS_RUNTIME_H
#define DSE_PHASE1_CPP_BUSINESS_RUNTIME_H

#include <functional>
#include "engine/ecs/world.h"

namespace phase1::runtime {

struct CppBusinessHooks {
    std::function<void(Phase1World&)> bootstrap;
    std::function<void(Phase1World&, float)> tick;
    std::function<void()> shutdown;
};

void ConfigureCppBusinessHooks(CppBusinessHooks hooks);
bool BootstrapCppBusiness(Phase1World& world);
void TickCppBusiness(Phase1World& world, float delta_time);
void ShutdownCppBusiness();

}

#endif
