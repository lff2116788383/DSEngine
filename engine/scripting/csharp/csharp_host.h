#pragma once
// ============================================================
// CSharpHost — .NET 8 CoreCLR embedding via hostfxr
// Manages C# scripting runtime lifecycle:
//   initialize → start → update/fixed_update per frame → reload → shutdown
// ============================================================

#include <string>
#include <cstdint>

// Forward declarations for hostfxr types (avoid including nethost headers in header)
typedef void* hostfxr_handle;

class CSharpHost {
public:
    CSharpHost() = default;
    ~CSharpHost();

    CSharpHost(const CSharpHost&) = delete;
    CSharpHost& operator=(const CSharpHost&) = delete;

    /// Initialize .NET runtime and load the game assembly.
    /// @param runtime_config_path Path to DSEngine.Runtime.runtimeconfig.json
    /// @param runtime_dll_path    Path to DSEngine.Runtime.dll
    /// @param game_dll_path       Path to DSEngine.Game.dll
    /// @return true on success
    bool initialize(const std::string& runtime_config_path,
                    const std::string& runtime_dll_path,
                    const std::string& game_dll_path);

    /// Shutdown the .NET runtime.
    void shutdown();

    /// Hot-reload: unload game assembly and reload from path.
    bool reload(const std::string& game_dll_path);

    /// Call all scripts' OnStart (idempotent — only fires once until reload).
    void invoke_start();

    /// Call all scripts' OnUpdate(dt).
    void invoke_update(float dt);

    /// Call all scripts' OnFixedUpdate(dt).
    void invoke_fixed_update(float dt);

    bool is_loaded() const { return host_context_ != nullptr; }

private:
    hostfxr_handle host_context_ = nullptr;

    // Function pointers into managed code (obtained via hostfxr)
    using fn_initialize   = int(*)(const char* path, int len);
    using fn_void         = void(*)();
    using fn_float        = void(*)(float);
    using fn_reload       = int(*)(const char* path, int len);

    fn_initialize managed_initialize_  = nullptr;
    fn_void       managed_start_       = nullptr;
    fn_float      managed_update_      = nullptr;
    fn_float      managed_fixed_       = nullptr;
    fn_reload     managed_reload_      = nullptr;
    fn_void       managed_shutdown_    = nullptr;

    bool load_hostfxr();
    bool get_managed_entry_points(const std::string& runtime_dll_path);
};
