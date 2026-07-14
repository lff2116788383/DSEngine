// ============================================================
// CSharpHost end-to-end test (real .NET 8 / hostfxr / CoreCLR)
//
// Drives the full managed lifecycle through the native host and asserts
// externally observable behaviour:
//   * Play init + Start/Update/FixedUpdate actually run managed script code
//     (verified via HostProbe counters surfaced through Callbacks.Query).
//   * Hot reload swaps to a *recompiled* game assembly and the new code takes
//     effect (build tag 1 -> 2).
//   * The previous collectible AssemblyLoadContext is reclaimed after GC
//     (WeakReference liveness == 0), i.e. no managed leak across reloads.
//
// Unlike a source-level check this requires the real runtime: the assertions
// only pass if managed code executed and the ALC was genuinely collected.
// ============================================================

#include "engine/scripting/csharp/csharp_host.h"

#include <cstdio>
#include <string>

#ifndef DSE_CS_MANAGED_DIR
#define DSE_CS_MANAGED_DIR "managed"
#endif
#ifndef DSE_CS_MANAGED_V2_DIR
#define DSE_CS_MANAGED_V2_DIR "managed_probe_v2"
#endif

namespace {

int g_failures = 0;

void check_eq(const char* what, long long expected, long long actual) {
    if (expected != actual) {
        std::fprintf(stderr, "[csharp-e2e] FAIL %s: expected %lld, got %lld\n",
                     what, expected, actual);
        ++g_failures;
    } else {
        std::printf("[csharp-e2e] OK   %s == %lld\n", what, actual);
    }
}

void check_ge(const char* what, long long minimum, long long actual) {
    if (actual < minimum) {
        std::fprintf(stderr, "[csharp-e2e] FAIL %s: expected >= %lld, got %lld\n",
                     what, minimum, actual);
        ++g_failures;
    } else {
        std::printf("[csharp-e2e] OK   %s == %lld (>= %lld)\n", what, actual, minimum);
    }
}

} // namespace

int main() {
    const std::string dir = DSE_CS_MANAGED_DIR;
    const std::string dir_v2 = DSE_CS_MANAGED_V2_DIR;

    const std::string config = dir + "/DSEngine.Runtime.runtimeconfig.json";
    const std::string runtime = dir + "/DSEngine.Runtime.dll";
    const std::string game_v1 = dir + "/DSEngine.Game.dll";
    const std::string game_v2 = dir_v2 + "/DSEngine.Game.dll";

    CSharpHost host;
    if (!host.initialize(config, runtime, game_v1)) {
        std::fprintf(stderr, "[csharp-e2e] initialize failed (config=%s)\n",
                     config.c_str());
        return 1;
    }
    if (!host.is_loaded()) {
        std::fprintf(stderr, "[csharp-e2e] host not loaded after initialize\n");
        return 2;
    }
    if (host.query(1) < 0) {
        std::fprintf(stderr,
                     "[csharp-e2e] Callbacks.Query unavailable — stale runtime dll?\n");
        return 3;
    }

    // Play init: scripts discovered but Start not yet fired.
    check_ge("scripts discovered (v1)", 1, host.query(6));
    check_eq("StartCount before Start", 0, host.query(1));

    host.invoke_start();
    check_ge("StartCount after Start", 1, host.query(1));
    check_eq("build tag after v1 Start", 1, host.query(5));

    host.invoke_update(0.016f);
    host.invoke_update(0.016f);
    host.invoke_update(0.016f);
    check_eq("UpdateCount after 3 updates", 3, host.query(2));

    host.invoke_fixed_update(0.02f);
    host.invoke_fixed_update(0.02f);
    check_eq("FixedUpdateCount after 2 fixed", 2, host.query(3));

    const long long start_before_reload = host.query(1);

    // Hot reload to the recompiled v2 assembly (PROBE_V2 => build tag 2).
    if (!host.reload(game_v2)) {
        std::fprintf(stderr, "[csharp-e2e] reload to v2 failed (%s)\n",
                     game_v2.c_str());
        return 4;
    }
    check_eq("build tag after v2 reload (new code active)", 2, host.query(5));
    check_ge("StartCount grew across reload", start_before_reload + 1, host.query(1));
    check_ge("DestroyCount after reload", 1, host.query(4));

    // Collectible ALC reclamation: the v1 context must be gone after GC.
    check_eq("unloaded ALCs tracked", 1, host.query(8));
    check_eq("unloaded ALCs still alive after GC (reclaimed)", 0, host.query(7));

    host.invoke_update(0.016f);
    check_eq("UpdateCount continues on v2", 4, host.query(2));

    host.shutdown();
    if (host.is_loaded()) {
        std::fprintf(stderr, "[csharp-e2e] host still loaded after shutdown\n");
        return 5;
    }

    if (g_failures != 0) {
        std::fprintf(stderr, "[csharp-e2e] %d assertion(s) failed\n", g_failures);
        return 6;
    }

    std::printf("CSHARP_HOST_E2E_PASS\n");
    return 0;
}
