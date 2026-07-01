using System;
using System.Collections.Generic;
using System.Reflection;

namespace DSEngine;

/// <summary>
/// Discovers and manages DseScript instances.
/// Called from the native host via UnmanagedCallersOnly entry points.
/// </summary>
public static class ScriptRegistry {
    private static readonly List<DseScript> _scripts = new();
    private static bool _started;

    /// <summary>Scan loaded assemblies for DseScript subclasses and instantiate them.</summary>
    public static void DiscoverAndInstantiate(Assembly? gameAssembly = null) {
        _scripts.Clear();
        _started = false;

        var assemblies = gameAssembly != null
            ? new[] { gameAssembly }
            : AppDomain.CurrentDomain.GetAssemblies();

        foreach (var asm in assemblies) {
            foreach (var type in asm.GetTypes()) {
                if (type.IsAbstract || !type.IsSubclassOf(typeof(DseScript)))
                    continue;

                if (Activator.CreateInstance(type) is DseScript script) {
                    _scripts.Add(script);
                }
            }
        }
    }

    /// <summary>Invoke OnStart on all registered scripts (once).</summary>
    public static void InvokeStart() {
        if (_started) return;
        _started = true;
        foreach (var s in _scripts) {
            try { s.OnStart(); }
            catch (Exception ex) { Console.Error.WriteLine($"[DSE-CS] OnStart error: {ex}"); }
        }
    }

    /// <summary>Invoke OnUpdate on all registered scripts.</summary>
    public static void InvokeUpdate(float dt) {
        foreach (var s in _scripts) {
            try { s.OnUpdate(dt); }
            catch (Exception ex) { Console.Error.WriteLine($"[DSE-CS] OnUpdate error: {ex}"); }
        }
    }

    /// <summary>Invoke OnFixedUpdate on all registered scripts.</summary>
    public static void InvokeFixedUpdate(float dt) {
        foreach (var s in _scripts) {
            try { s.OnFixedUpdate(dt); }
            catch (Exception ex) { Console.Error.WriteLine($"[DSE-CS] OnFixedUpdate error: {ex}"); }
        }
    }

    /// <summary>Destroy all scripts.</summary>
    public static void DestroyAll() {
        foreach (var s in _scripts) {
            try { s.OnDestroy(); }
            catch (Exception ex) { Console.Error.WriteLine($"[DSE-CS] OnDestroy error: {ex}"); }
        }
        _scripts.Clear();
        _started = false;
    }

    /// <summary>Get count of active scripts.</summary>
    public static int Count => _scripts.Count;
}
