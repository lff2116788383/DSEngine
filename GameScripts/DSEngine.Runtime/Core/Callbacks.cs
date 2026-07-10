using System;
using System.IO;
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
            _gameAlc?.Unload();
            _gameAlc = null;
            _gameAssembly = null;

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
        _gameAlc?.Unload();
        _gameAlc = null;
        _gameAssembly = null;
    }
}
