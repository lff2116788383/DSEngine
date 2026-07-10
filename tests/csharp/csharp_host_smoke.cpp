// ============================================================
// csharp_host_smoke — C# 脚本运行时端到端无头冒烟
//
// 通过 CSharpHost（hostfxr → CoreCLR）加载托管程序集并驱动一帧回调，
// 覆盖：hostfxr 定位 → runtimeconfig 初始化 → 托管入口点解析 →
// Initialize(加载 Game 程序集) → Start/Update/FixedUpdate → Reload(ALC 热重载) → Shutdown。
// 退出码 0 = 通过。仅在 -DDSE_ENABLE_CSHARP=ON 且已执行 dse_csharp_build 时运行。
// ============================================================

#include "engine/scripting/csharp/csharp_host.h"

#include <cstdio>
#include <string>

#ifndef DSE_CS_MANAGED_DIR
#define DSE_CS_MANAGED_DIR "managed"
#endif

int main() {
    const std::string dir     = DSE_CS_MANAGED_DIR;
    const std::string config  = dir + "/DSEngine.Runtime.runtimeconfig.json";
    const std::string runtime = dir + "/DSEngine.Runtime.dll";
    const std::string game    = dir + "/DSEngine.Game.dll";

    CSharpHost host;
    if (!host.initialize(config, runtime, game)) {
        std::fprintf(stderr, "[csharp-smoke] initialize failed (config=%s)\n", config.c_str());
        return 1;
    }
    if (!host.is_loaded()) {
        std::fprintf(stderr, "[csharp-smoke] host not loaded after initialize\n");
        return 2;
    }

    // 驱动一帧生命周期
    host.invoke_start();
    host.invoke_update(0.016f);
    host.invoke_fixed_update(0.016f);

    // 热重载往返（AssemblyLoadContext 卸载 + 重新加载 Game 程序集）
    if (!host.reload(game)) {
        std::fprintf(stderr, "[csharp-smoke] reload failed\n");
        return 3;
    }
    host.invoke_update(0.016f);

    host.shutdown();
    if (host.is_loaded()) {
        std::fprintf(stderr, "[csharp-smoke] host still loaded after shutdown\n");
        return 4;
    }

    std::printf("CSHARP_HOST_SMOKE_PASS\n");
    return 0;
}
