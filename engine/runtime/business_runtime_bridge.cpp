#include "engine/runtime/business_runtime_bridge.h"

#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"

namespace dse::runtime {

bool BootstrapBusinessRuntime(RuntimeContext& context, const RuntimeStatsBindings& stats_bindings) {
    if (context.business_mode == BusinessMode::Lua) {
#ifdef DSE_ENABLE_LUA
        ConfigureLuaApiContext({
            context.world,
            context.window_title_setter,
            stats_bindings.get_draw_calls,
            stats_bindings.get_max_batch_sprites,
            stats_bindings.get_sprite_count,
            context.asset_manager,
            context.audio_system,
            context.quit_app,
            context.set_target_fps,
            context.get_target_fps
        });
        return BootstrapLuaRuntime();
#else
        DEBUG_LOG_ERROR("BusinessMode::Lua requested but DSE_ENABLE_LUA is OFF");
        return false;
#endif
    }

    if (context.world == nullptr || context.asset_manager == nullptr) {
        return false;
    }
    return BootstrapCppBusiness(*context.world, *context.asset_manager);
}

void TickBusinessRuntime(RuntimeContext& context, float delta_time) {
    if (context.business_mode == BusinessMode::Lua) {
#ifdef DSE_ENABLE_LUA
        TickLuaRuntime(delta_time);
#endif
        return;
    }

    if (context.world == nullptr) {
        return;
    }
    TickCppBusiness(*context.world, delta_time);
}

void ShutdownBusinessRuntime(const RuntimeContext& context) {
    if (context.business_mode == BusinessMode::Lua) {
#ifdef DSE_ENABLE_LUA
        ShutdownLuaRuntime();
#endif
        return;
    }
    ShutdownCppBusiness();
}

} // namespace dse::runtime
