using DSEngine;

namespace DSEngine.Game;

/// <summary>
/// Deterministic script used by the C# host end-to-end test. It records lifecycle
/// invocations into <see cref="HostProbe"/> (host load context) and stamps a
/// build tag so a hot reload from a recompiled assembly is externally observable:
/// the default build reports tag 1, a build with the PROBE_V2 constant reports 2.
/// It intentionally performs no native interop so behaviour is stable regardless
/// of which host executable loaded the engine.
/// </summary>
public partial class ProbeScript : DseScript {
#if PROBE_V2
    private const long BuildTag = 2;
#else
    private const long BuildTag = 1;
#endif

    public override void OnStart() {
        HostProbe.StartCount++;
        HostProbe.LastTag = BuildTag;
    }

    public override void OnUpdate(float dt) {
        HostProbe.UpdateCount++;
    }

    public override void OnFixedUpdate(float dt) {
        HostProbe.FixedUpdateCount++;
    }

    public override void OnDestroy() {
        HostProbe.DestroyCount++;
    }
}
