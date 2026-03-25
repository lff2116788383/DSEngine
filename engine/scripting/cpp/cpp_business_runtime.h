#ifndef DSE_CPP_BUSINESS_RUNTIME_H
#define DSE_CPP_BUSINESS_RUNTIME_H

#include <functional>
#include "engine/ecs/world.h"

namespace dse::runtime {

struct CppBusinessHooks {
    std::function<void(World&)> bootstrap;
    std::function<void(World&, float)> tick;
    std::function<void()> shutdown;
};

void ConfigureCppBusinessHooks(CppBusinessHooks hooks);
bool BootstrapCppBusiness(World& world);
void TickCppBusiness(World& world, float delta_time);
void ShutdownCppBusiness();

}

#endif
