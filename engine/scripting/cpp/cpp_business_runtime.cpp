/**
 * @file cpp_business_runtime.cpp
 * @brief 时间管理系统，提供高精度计时器、增量时间(Delta Time)计算
 */

#include "engine/scripting/cpp/cpp_business_runtime.h"
#include "engine/base/debug.h"
#include <utility>

namespace dse::runtime {
namespace {
CppBusinessHooks& Hooks() {
    static CppBusinessHooks hooks;
    return hooks;
}
}

void ConfigureCppBusinessHooks(CppBusinessHooks hooks) {
    Hooks() = std::move(hooks);
}

bool BootstrapCppBusiness(World& world, AssetManager& asset_manager) {
    auto& hooks = Hooks();
    if (!hooks.bootstrap || !hooks.tick) {
        DEBUG_LOG_ERROR("Cpp business bootstrap failed: hooks are not configured");
        return false;
    }
    hooks.bootstrap(world, asset_manager);
    return true;
}

void TickCppBusiness(World& world, float delta_time) {
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
