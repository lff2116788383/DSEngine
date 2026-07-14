using System;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

namespace DSEngine;

/// <summary>
/// Entry points exported to the native host via [UnmanagedCallersOnly].
/// The C++ CSharpHost calls these through function pointers obtained from hostfxr.
/// </summary>
public static class Callbacks {
    private static AssemblyLoadContext? _gameAlc;
    private static Assembly? _gameAssembly;

    // Weak references to game ALCs that have been unloaded. Used by the E2E test
    // to verify collectible contexts are actually reclaimed after GC.
    private static readonly List<WeakReference> _unloadedAlcs = new();

    // Record the current game ALC as unloaded (weakly, so it can be collected)
    // and drop the strong references. Kept in a no-inline helper so the JIT does
    // not extend the lifetime of the local past the collection point.
    [MethodImpl(MethodImplOptions.NoInlining)]
    private static void UnloadCurrentGameAlc() {
        if (_gameAlc == null) return;
        _gameAlc.Unload();
        _unloadedAlcs.Add(new WeakReference(_gameAlc));
        _gameAlc = null;
        _gameAssembly = null;
    }

    /// <summary>
    /// Initialize the scripting runtime: load game assembly and discover scripts.
    /// Called by native host after hostfxr setup.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Initialize(IntPtr gameAssemblyPathPtr, int pathLength) {
        try {
            string? path = Marshal.PtrToStringUTF8(gameAssemblyPathPtr, pathLength);
            if (string.IsNullOrEmpty(path)) return -1;

            _gameAlc = CreateGameAlc(path);
            _gameAssembly = _gameAlc.LoadFromAssemblyPath(path);
            ScriptRegistry.DiscoverAndInstantiate(_gameAssembly);
            return 0;
        } catch (Exception ex) {
            Console.Error.WriteLine($"[DSE-CS] Initialize failed: {ex}");
            return -1;
        }
    }

    /// <summary>
    /// 为游戏程序集创建可回收的加载上下文，并挂接依赖解析：
    /// 1) 引用到 DSEngine.Runtime 等已加载程序集时，复用宿主组件 ALC 中的同名程序集，
    ///    保证 Script 基类等类型标识在两个 ALC 间一致（否则类型不匹配、转换失败）。
    /// 2) 其余依赖从游戏程序集所在目录按名加载。
    /// </summary>
    private static AssemblyLoadContext CreateGameAlc(string gameAssemblyPath) {
        var alc = new AssemblyLoadContext("DSEngine.Game", isCollectible: true);
        var hostAlc = AssemblyLoadContext.GetLoadContext(typeof(Callbacks).Assembly);
        string? gameDir = Path.GetDirectoryName(gameAssemblyPath);
        alc.Resolving += (ctx, name) => {
            if (hostAlc != null) {
                foreach (var asm in hostAlc.Assemblies) {
                    if (asm.GetName().Name == name.Name) return asm;
                }
            }
            if (!string.IsNullOrEmpty(gameDir)) {
                string candidate = Path.Combine(gameDir, name.Name + ".dll");
                if (File.Exists(candidate)) return ctx.LoadFromAssemblyPath(candidate);
            }
            return null;
        };
        return alc;
    }

    /// <summary>Invoke OnStart on all scripts.</summary>
    [UnmanagedCallersOnly]
    public static void Start() {
        ScriptRegistry.InvokeStart();
    }

    /// <summary>Invoke OnUpdate on all scripts.</summary>
    [UnmanagedCallersOnly]
    public static void Update(float dt) {
        ScriptRegistry.InvokeUpdate(dt);
    }

    /// <summary>Invoke OnFixedUpdate on all scripts.</summary>
    [UnmanagedCallersOnly]
    public static void FixedUpdate(float dt) {
        ScriptRegistry.InvokeFixedUpdate(dt);
    }

    /// <summary>
    /// Hot-reload: unload current game assembly, reload from new path.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Reload(IntPtr gameAssemblyPathPtr, int pathLength) {
        try {
            ScriptRegistry.DestroyAll();
            UnloadCurrentGameAlc();

            GC.Collect();
            GC.WaitForPendingFinalizers();

            string? path = Marshal.PtrToStringUTF8(gameAssemblyPathPtr, pathLength);
            if (string.IsNullOrEmpty(path)) return -1;

            _gameAlc = CreateGameAlc(path);
            _gameAssembly = _gameAlc.LoadFromAssemblyPath(path);
            ScriptRegistry.DiscoverAndInstantiate(_gameAssembly);
            ScriptRegistry.InvokeStart();
            return 0;
        } catch (Exception ex) {
            Console.Error.WriteLine($"[DSE-CS] Reload failed: {ex}");
            return -1;
        }
    }

    /// <summary>Shutdown: destroy all scripts and unload.</summary>
    [UnmanagedCallersOnly]
    public static void Shutdown() {
        ScriptRegistry.DestroyAll();
        UnloadCurrentGameAlc();
    }

    /// <summary>
    /// Diagnostics/test query surfaced to the native host. Returns observable
    /// runtime state without exposing managed object references. Keys:
    ///   1 StartCount, 2 UpdateCount, 3 FixedUpdateCount, 4 DestroyCount,
    ///   5 LastTag, 6 active script count,
    ///   7 unloaded game ALCs still alive after a forced GC (0 == all reclaimed),
    ///   8 total unloaded game ALCs tracked.
    /// </summary>
    [UnmanagedCallersOnly]
    public static long Query(int key) {
        switch (key) {
            case 1: return HostProbe.StartCount;
            case 2: return HostProbe.UpdateCount;
            case 3: return HostProbe.FixedUpdateCount;
            case 4: return HostProbe.DestroyCount;
            case 5: return HostProbe.LastTag;
            case 6: return ScriptRegistry.Count;
            case 7: return CountAliveUnloadedAlcs();
            case 8: return _unloadedAlcs.Count;
            default: return -1;
        }
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static long CountAliveUnloadedAlcs() {
        for (int i = 0; i < 10; i++) {
            GC.Collect();
            GC.WaitForPendingFinalizers();
        }
        long alive = 0;
        foreach (var wr in _unloadedAlcs) {
            if (wr.IsAlive) alive++;
        }
        return alive;
    }
}
