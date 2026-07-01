using System;
using System.Reflection;
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

    /// <summary>
    /// Initialize the scripting runtime: load game assembly and discover scripts.
    /// Called by native host after hostfxr setup.
    /// </summary>
    [UnmanagedCallersOnly]
    public static int Initialize(IntPtr gameAssemblyPathPtr, int pathLength) {
        try {
            string? path = Marshal.PtrToStringUTF8(gameAssemblyPathPtr, pathLength);
            if (string.IsNullOrEmpty(path)) return -1;

            _gameAlc = new AssemblyLoadContext("DSEngine.Game", isCollectible: true);
            _gameAssembly = _gameAlc.LoadFromAssemblyPath(path);
            ScriptRegistry.DiscoverAndInstantiate(_gameAssembly);
            return 0;
        } catch (Exception ex) {
            Console.Error.WriteLine($"[DSE-CS] Initialize failed: {ex}");
            return -1;
        }
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
            _gameAlc?.Unload();
            _gameAlc = null;
            _gameAssembly = null;

            GC.Collect();
            GC.WaitForPendingFinalizers();

            string? path = Marshal.PtrToStringUTF8(gameAssemblyPathPtr, pathLength);
            if (string.IsNullOrEmpty(path)) return -1;

            _gameAlc = new AssemblyLoadContext("DSEngine.Game", isCollectible: true);
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
        _gameAlc?.Unload();
        _gameAlc = null;
        _gameAssembly = null;
    }
}
