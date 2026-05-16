#include "engine/runtime/business_runtime_bridge.h"

#include "engine/assets/asset_manager.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"

namespace dse::runtime {

bool BootstrapBusinessRuntime(RuntimeContext& context, const RuntimeStatsBindings& stats_bindings) {
    if (context.business_mode == BusinessMode::Lua) {
        ConfigureLuaApiContext({
            context.world,
            context.window_title_setter,
            stats_bindings.get_draw_calls,
            stats_bindings.get_max_batch_sprites,
            stats_bindings.get_sprite_count,
            context.asset_manager,
            context.audio_system
        });
        return BootstrapLuaRuntime();
    }

    if (context.world == nullptr || context.asset_manager == nullptr) {
        return false;
    }
    return BootstrapCppBusiness(*context.world, *context.asset_manager);
}

void TickBusinessRuntime(RuntimeContext& context, float delta_time) {
    if (context.business_mode == BusinessMode::Lua) {
        TickLuaRuntime(delta_time);
        return;
    }

    if (context.world == nullptr) {
        return;
    }
    TickCppBusiness(*context.world, delta_time);
}

void ShutdownBusinessRuntime(const RuntimeContext& context) {
    if (context.business_mode == BusinessMode::Lua) {
        ShutdownLuaRuntime();
        return;
    }
    ShutdownCppBusiness();
}

} // namespace dse::runtime
