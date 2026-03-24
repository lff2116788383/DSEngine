#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "engine/base/debug.h"
#include <utility>

namespace phase1::runtime {
namespace {
CppBusinessHooks& Hooks() {
    static CppBusinessHooks hooks;
    return hooks;
}
}

void ConfigureCppBusinessHooks(CppBusinessHooks hooks) {
    Hooks() = std::move(hooks);
}

bool BootstrapCppBusiness(Phase1World& world) {
    auto& hooks = Hooks();
    if (!hooks.bootstrap || !hooks.tick) {
        DEBUG_LOG_ERROR("Phase1 cpp business bootstrap failed: hooks are not configured");
        return false;
    }
    hooks.bootstrap(world);
    return true;
}

void TickCppBusiness(Phase1World& world, float delta_time) {
    auto& hooks = Hooks();
    if (!hooks.tick) {
        return;
    }
    hooks.tick(world, delta_time);
}

void ShutdownCppBusiness() {
    auto& hooks = Hooks();
    if (hooks.shutdown) {
        hooks.shutdown();
    }
}
}
