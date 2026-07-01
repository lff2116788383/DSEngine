// ============================================================
// CSharpHost — .NET 8 CoreCLR embedding implementation
// Uses hostfxr API to initialize .NET runtime and obtain
// function pointers to managed [UnmanagedCallersOnly] methods.
// ============================================================

#include "csharp_host.h"

#ifdef DSE_CSHARP

#include <iostream>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <Windows.h>
#define DSE_STR(s) L##s
typedef wchar_t char_t;
#else
#define DSE_STR(s) s
typedef char char_t;
#endif

// ── nethost / hostfxr API declarations ──────────────────────────────────────
// We load these dynamically to avoid hard link-time dependency on a specific
// .NET SDK install location.

#include <nethost.h>
#include <coreclr_delegates.h>
#include <hostfxr.h>

// hostfxr function pointer types
using hostfxr_initialize_for_runtime_config_fn =
    int(*)(const char_t* runtime_config_path, const void* parameters, hostfxr_handle* host_context_handle);
using hostfxr_get_runtime_delegate_fn =
    int(*)(const hostfxr_handle host_context_handle, int type, void** delegate);
using hostfxr_close_fn = int(*)(const hostfxr_handle host_context_handle);

// Loaded function pointers
static hostfxr_initialize_for_runtime_config_fn s_hostfxr_init   = nullptr;
static hostfxr_get_runtime_delegate_fn          s_hostfxr_get_delegate = nullptr;
static hostfxr_close_fn                         s_hostfxr_close  = nullptr;

// .NET delegate type for load_assembly_and_get_function_pointer
static load_assembly_and_get_function_pointer_fn s_load_assembly = nullptr;

// ── Helpers ─────────────────────────────────────────────────────────────────

#ifdef _WIN32
static std::wstring to_wide(const std::string& s) {
    if (s.empty()) return {};
    int sz = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring result(sz, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), result.data(), sz);
    return result;
}
#endif

// ── Load hostfxr dynamically ────────────────────────────────────────────────

bool CSharpHost::load_hostfxr() {
    // Get hostfxr path from nethost
    char_t buffer[4096];
    size_t buffer_size = sizeof(buffer) / sizeof(char_t);
    int rc = get_hostfxr_path(buffer, &buffer_size, nullptr);
    if (rc != 0) {
        std::cerr << "[DSE-CS] Failed to find hostfxr. Is .NET 8 SDK installed?\n";
        return false;
    }

#ifdef _WIN32
    HMODULE lib = LoadLibraryW(buffer);
    if (!lib) {
        std::cerr << "[DSE-CS] Failed to load hostfxr.dll\n";
        return false;
    }
    s_hostfxr_init = (hostfxr_initialize_for_runtime_config_fn)
        GetProcAddress(lib, "hostfxr_initialize_for_runtime_config");
    s_hostfxr_get_delegate = (hostfxr_get_runtime_delegate_fn)
        GetProcAddress(lib, "hostfxr_get_runtime_delegate");
    s_hostfxr_close = (hostfxr_close_fn)
        GetProcAddress(lib, "hostfxr_close");
#else
    void* lib = dlopen(buffer, RTLD_NOW | RTLD_LOCAL);
    if (!lib) {
        std::cerr << "[DSE-CS] Failed to load hostfxr: " << dlerror() << "\n";
        return false;
    }
    s_hostfxr_init = (hostfxr_initialize_for_runtime_config_fn)dlsym(lib, "hostfxr_initialize_for_runtime_config");
    s_hostfxr_get_delegate = (hostfxr_get_runtime_delegate_fn)dlsym(lib, "hostfxr_get_runtime_delegate");
    s_hostfxr_close = (hostfxr_close_fn)dlsym(lib, "hostfxr_close");
#endif

    return s_hostfxr_init && s_hostfxr_get_delegate && s_hostfxr_close;
}

// ── Get managed entry points ────────────────────────────────────────────────

bool CSharpHost::get_managed_entry_points(const std::string& runtime_dll_path) {
#ifdef _WIN32
    std::wstring dll_w = to_wide(runtime_dll_path);
    const auto* assembly_path = dll_w.c_str();
#else
    const auto* assembly_path = runtime_dll_path.c_str();
#endif

    // Type: DSEngine.Callbacks, DSEngine.Runtime
    // Method names correspond to [UnmanagedCallersOnly] methods in Callbacks.cs
#ifdef _WIN32
    #define DSE_TYPE  L"DSEngine.Callbacks, DSEngine.Runtime"
    #define DSE_METHOD_INIT  L"Initialize"
    #define DSE_METHOD_START L"Start"
    #define DSE_METHOD_UPDATE L"Update"
    #define DSE_METHOD_FIXED  L"FixedUpdate"
    #define DSE_METHOD_RELOAD L"Reload"
    #define DSE_METHOD_SHUTDOWN L"Shutdown"
#else
    #define DSE_TYPE  "DSEngine.Callbacks, DSEngine.Runtime"
    #define DSE_METHOD_INIT  "Initialize"
    #define DSE_METHOD_START "Start"
    #define DSE_METHOD_UPDATE "Update"
    #define DSE_METHOD_FIXED  "FixedUpdate"
    #define DSE_METHOD_RELOAD "Reload"
    #define DSE_METHOD_SHUTDOWN "Shutdown"
#endif

    // UNMANAGEDCALLERSONLY_METHOD is the delegate type for [UnmanagedCallersOnly]
    int rc = 0;
    rc = s_load_assembly(assembly_path, DSE_TYPE, DSE_METHOD_INIT,
                         UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&managed_initialize_);
    if (rc != 0 || !managed_initialize_) {
        std::cerr << "[DSE-CS] Failed to get Callbacks.Initialize\n";
        return false;
    }

    s_load_assembly(assembly_path, DSE_TYPE, DSE_METHOD_START,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&managed_start_);
    s_load_assembly(assembly_path, DSE_TYPE, DSE_METHOD_UPDATE,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&managed_update_);
    s_load_assembly(assembly_path, DSE_TYPE, DSE_METHOD_FIXED,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&managed_fixed_);
    s_load_assembly(assembly_path, DSE_TYPE, DSE_METHOD_RELOAD,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&managed_reload_);
    s_load_assembly(assembly_path, DSE_TYPE, DSE_METHOD_SHUTDOWN,
                    UNMANAGEDCALLERSONLY_METHOD, nullptr, (void**)&managed_shutdown_);

    return managed_start_ && managed_update_ && managed_fixed_ && managed_reload_ && managed_shutdown_;
}

// ── Public API ──────────────────────────────────────────────────────────────

bool CSharpHost::initialize(const std::string& runtime_config_path,
                            const std::string& runtime_dll_path,
                            const std::string& game_dll_path) {
    if (!load_hostfxr()) return false;

    // Initialize .NET runtime
#ifdef _WIN32
    std::wstring config_w = to_wide(runtime_config_path);
    int rc = s_hostfxr_init(config_w.c_str(), nullptr, &host_context_);
#else
    int rc = s_hostfxr_init(runtime_config_path.c_str(), nullptr, &host_context_);
#endif

    if (rc != 0 || !host_context_) {
        std::cerr << "[DSE-CS] hostfxr_initialize_for_runtime_config failed: 0x"
                  << std::hex << rc << std::dec << "\n";
        return false;
    }

    // Get load_assembly_and_get_function_pointer delegate
    rc = s_hostfxr_get_delegate(
        host_context_,
        hdt_load_assembly_and_get_function_pointer,
        (void**)&s_load_assembly);

    if (rc != 0 || !s_load_assembly) {
        std::cerr << "[DSE-CS] Failed to get load_assembly_and_get_function_pointer\n";
        shutdown();
        return false;
    }

    // Get managed entry points
    if (!get_managed_entry_points(runtime_dll_path)) {
        shutdown();
        return false;
    }

    // Initialize: load game assembly
    int init_rc = managed_initialize_(game_dll_path.c_str(), (int)game_dll_path.size());
    if (init_rc != 0) {
        std::cerr << "[DSE-CS] Managed Initialize() failed\n";
        shutdown();
        return false;
    }

    std::cout << "[DSE-CS] C# runtime initialized successfully\n";
    return true;
}

void CSharpHost::shutdown() {
    if (managed_shutdown_) {
        managed_shutdown_();
    }
    if (host_context_) {
        s_hostfxr_close(host_context_);
        host_context_ = nullptr;
    }
    managed_initialize_ = nullptr;
    managed_start_ = nullptr;
    managed_update_ = nullptr;
    managed_fixed_ = nullptr;
    managed_reload_ = nullptr;
    managed_shutdown_ = nullptr;
    std::cout << "[DSE-CS] C# runtime shut down\n";
}

CSharpHost::~CSharpHost() {
    if (is_loaded()) shutdown();
}

bool CSharpHost::reload(const std::string& game_dll_path) {
    if (!managed_reload_) return false;
    int rc = managed_reload_(game_dll_path.c_str(), (int)game_dll_path.size());
    if (rc != 0) {
        std::cerr << "[DSE-CS] Hot-reload failed\n";
        return false;
    }
    std::cout << "[DSE-CS] Hot-reload successful\n";
    return true;
}

void CSharpHost::invoke_start() {
    if (managed_start_) managed_start_();
}

void CSharpHost::invoke_update(float dt) {
    if (managed_update_) managed_update_(dt);
}

void CSharpHost::invoke_fixed_update(float dt) {
    if (managed_fixed_) managed_fixed_(dt);
}

#endif // DSE_CSHARP
