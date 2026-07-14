namespace DSEngine;

/// <summary>
/// Observable counters that live in the host (DSEngine.Runtime) load context and
/// therefore survive game-assembly hot reloads. Game scripts (loaded into the
/// collectible game ALC) write to these fields; the native host reads them back
/// through <see cref="Callbacks.Query"/>. Only primitives are stored so the game
/// ALC never becomes reachable through this class and stays collectible.
/// </summary>
public static class HostProbe {
    public static int StartCount;
    public static int UpdateCount;
    public static int FixedUpdateCount;
    public static int DestroyCount;
    public static long LastTag;

    public static void Reset() {
        StartCount = 0;
        UpdateCount = 0;
        FixedUpdateCount = 0;
        DestroyCount = 0;
        LastTag = 0;
    }
}
