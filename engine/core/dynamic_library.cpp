#include "engine/core/dynamic_library.h"
#include "engine/base/debug.h"
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace dse {
namespace core {

DynamicLibrary::DynamicLibrary() = default;

DynamicLibrary::~DynamicLibrary() {
    Unload();
}

DynamicLibrary::DynamicLibrary(DynamicLibrary&& other) noexcept : handle_(other.handle_), path_(std::move(other.path_)) {
    other.handle_ = nullptr;
}

DynamicLibrary& DynamicLibrary::operator=(DynamicLibrary&& other) noexcept {
    if (this != &other) {
        Unload();
        handle_ = other.handle_;
        path_ = std::move(other.path_);
        other.handle_ = nullptr;
    }
    return *this;
}

bool DynamicLibrary::Load(const std::string& library_name) {
    if (IsLoaded()) {
        DEBUG_LOG_WARN("Library {} is already loaded", path_);
        return true;
    }

    std::string full_path = library_name;
#if defined(_WIN32)
    if (full_path.find(".dll") == std::string::npos) {
        full_path += ".dll";
    }
    handle_ = LoadLibraryA(full_path.c_str());
    if (!handle_) {
        DWORD err = GetLastError();
        DEBUG_LOG_ERROR("Failed to load library {}. Error code: {}", full_path, err);
        return false;
    }
#elif defined(__APPLE__)
    if (full_path.find(".dylib") == std::string::npos) {
        full_path = "lib" + full_path + ".dylib";
    }
    handle_ = dlopen(full_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle_) {
        DEBUG_LOG_ERROR("Failed to load library {}: {}", full_path, dlerror());
        return false;
    }
#else
    if (full_path.find(".so") == std::string::npos) {
        full_path = "lib" + full_path + ".so";
    }
    handle_ = dlopen(full_path.c_str(), RTLD_LAZY | RTLD_LOCAL);
    if (!handle_) {
        DEBUG_LOG_ERROR("Failed to load library {}: {}", full_path, dlerror());
        return false;
    }
#endif

    path_ = full_path;
    DEBUG_LOG_INFO("Successfully loaded dynamic library: {}", path_);
    return true;
}

void DynamicLibrary::Unload() {
    if (!IsLoaded()) {
        return;
    }

#if defined(_WIN32)
    FreeLibrary(static_cast<HMODULE>(handle_));
#else
    dlclose(handle_);
#endif

    DEBUG_LOG_INFO("Unloaded dynamic library: {}", path_);
    handle_ = nullptr;
    path_.clear();
}

void* DynamicLibrary::GetSymbol(const std::string& symbol_name) const {
    if (!IsLoaded()) {
        DEBUG_LOG_ERROR("Cannot get symbol {} because library is not loaded", symbol_name);
        return nullptr;
    }

#if defined(_WIN32)
    void* sym = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle_), symbol_name.c_str()));
#else
    void* sym = dlsym(handle_, symbol_name.c_str());
#endif

    if (!sym) {
        DEBUG_LOG_WARN("Symbol {} not found in library {}", symbol_name, path_);
    }
    return sym;
}

} // namespace core
} // namespace dse