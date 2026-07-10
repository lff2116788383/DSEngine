using System;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace DSEngine;

/// <summary>
/// 解析所有 [LibraryImport("dse_engine")] 的原生库。
///
/// C# 脚本运行时是「内嵌」在引擎宿主进程里的（编辑器 / 游戏运行时可执行文件
/// 静态链接了 dse_engine 并托管 CoreCLR）。默认构建下不存在独立的 dse_engine.dll，
/// C ABI 符号就在宿主可执行文件内。因此把库名 "dse_engine" 解析到「主程序句柄」
/// (GetMainProgramHandle)，让 P/Invoke 直接绑定宿主导出的符号；
/// 若引擎以 DSE_BUILD_SHARED=ON 构建（存在 dse_engine.dll），则回退到按名加载 DLL。
/// </summary>
internal static class NativeResolver {
    private const string Lib = "dse_engine";
    private static bool s_registered;

    [ModuleInitializer]
    internal static void Register() {
        if (s_registered) return;
        s_registered = true;
        NativeLibrary.SetDllImportResolver(typeof(NativeResolver).Assembly, Resolve);
    }

    // 引擎既可能以独立 DLL 形式存在（DSE_BUILD_SHARED=ON），也可能内嵌进宿主可执行文件
    // （默认静态构建 + 导出符号）。DLL 输出名为 DSEngine[_debug].dll（见 CMake OUTPUT_NAME
    // "DSEngine" / DEBUG_POSTFIX "_debug"），与 P/Invoke 名 "dse_engine" 不一致，故逐个尝试。
    private static readonly string[] DllCandidates = { "dse_engine", "DSEngine", "DSEngine_debug" };

    private static IntPtr Resolve(string libraryName, Assembly assembly, DllImportSearchPath? searchPath) {
        if (libraryName != Lib) return IntPtr.Zero;

        // 1) 独立/已加载的引擎 DLL。若引擎作为宿主的依赖 DLL 已在进程中，TryLoad 直接返回其句柄。
        foreach (string name in DllCandidates) {
            if (NativeLibrary.TryLoad(name, assembly, searchPath, out IntPtr dll)) return dll;
        }

        // 2) 内嵌宿主：C ABI 符号在主程序（可执行文件）导出表里。
        try {
            IntPtr main = NativeLibrary.GetMainProgramHandle();
            if (main != IntPtr.Zero) return main;
        } catch { /* 某些平台/宿主不支持 */ }

        return IntPtr.Zero;
    }
}
