#include "engine/runtime/business_runtime_bridge.h"

#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"
#include "engine/scripting/cpp/cpp_business_runtime.h"

#ifdef DSE_ENABLE_CSHARP
#include "engine/scripting/csharp/csharp_host.h"
#endif

namespace dse::runtime {

#ifdef DSE_ENABLE_CSHARP
namespace {
// 业务运行时持有的单例 C# 宿主（CoreCLR）。托管侧自行枚举脚本实例，
// 引擎每帧统一驱动 Start/Update 生命周期。
CSharpHost& CSharpBusinessHost() {
    static CSharpHost s_host;
    return s_host;
}

bool BootstrapCSharpBusiness(const RuntimeContext& context) {
    const std::string dir = context.csharp_managed_dir.empty() ? std::string("managed")
                                                               : context.csharp_managed_dir;
    const std::string config  = dir + "/DSEngine.Runtime.runtimeconfig.json";
    const std::string runtime = dir + "/DSEngine.Runtime.dll";
    const std::string game    = dir + "/DSEngine.Game.dll";

    auto& host = CSharpBusinessHost();
    if (!host.initialize(config, runtime, game)) {
        DEBUG_LOG_ERROR("BusinessMode::CSharp bootstrap failed (managed dir=%s)", dir.c_str());
        return false;
    }
    host.invoke_start();
    return true;
}
} // namespace
#endif

bool BootstrapBusinessRuntime(RuntimeContext& context, const RuntimeStatsBindings& stats_bindings) {
    if (context.business_mode == BusinessMode::Lua) {
#ifdef DSE_ENABLE_LUA
        ConfigureLuaApiContext({
            context.world,
            context.window_title_setter,
            stats_bindings.get_draw_calls,
            stats_bindings.get_max_batch_sprites,
            stats_bindings.get_sprite_count,
            stats_bindings.get_gpu_driven_active,
            stats_bindings.get_gpu_indirect_draw_count,
            stats_bindings.get_gpu_total_instances,
            context.asset_manager,
            context.audio_system,
            context.floating_origin,
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

    if (context.business_mode == BusinessMode::CSharp) {
#ifdef DSE_ENABLE_CSHARP
        return BootstrapCSharpBusiness(context);
#else
        DEBUG_LOG_ERROR("BusinessMode::CSharp requested but DSE_ENABLE_CSHARP is OFF");
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
        PumpLuaScriptHotReloads();
        TickLuaRuntime(delta_time);
#endif
        return;
    }

    if (context.business_mode == BusinessMode::CSharp) {
#ifdef DSE_ENABLE_CSHARP
        CSharpBusinessHost().invoke_update(delta_time);
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

    if (context.business_mode == BusinessMode::CSharp) {
#ifdef DSE_ENABLE_CSHARP
        CSharpBusinessHost().shutdown();
#endif
        return;
    }

    ShutdownCppBusiness();
}

} // namespace dse::runtime
