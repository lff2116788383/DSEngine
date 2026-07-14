#include <gtest/gtest.h>

#include "engine/core/dynamic_library.h"

#include <filesystem>
#include <system_error>

namespace {

namespace fs = std::filesystem;
using dse::core::DynamicLibrary;
using ProbeFn = int (*)();

fs::path SwapPluginOnDisk(const char* src, const fs::path& dst) {
    std::error_code ec;
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    EXPECT_FALSE(ec) << "copy_file failed: " << ec.message();
    return dst;
}

}  // namespace

// Proves the native-plugin hot-reload mechanism the editor relies on: load a
// plugin DLL, resolve and call an exported symbol, unload it, swap the DLL file
// on disk for a new build, reload, and observe the new behavior take effect.
TEST(NativePluginReload, ReloadPicksUpNewBehaviorAfterDiskSwap) {
    const fs::path dll = fs::temp_directory_path() / "dse_native_plugin_reload_probe.dll";
    std::error_code ec;

    SwapPluginOnDisk(DSE_TEST_PLUGIN_V1_DLL, dll);
    {
        DynamicLibrary lib;
        ASSERT_TRUE(lib.Load(dll.string()));
        ASSERT_TRUE(lib.IsLoaded());
        auto probe = reinterpret_cast<ProbeFn>(lib.GetSymbol("dse_test_plugin_probe"));
        ASSERT_NE(probe, nullptr);
        EXPECT_EQ(probe(), 1);
        lib.Unload();
        EXPECT_FALSE(lib.IsLoaded());
    }

    SwapPluginOnDisk(DSE_TEST_PLUGIN_V2_DLL, dll);
    {
        DynamicLibrary lib;
        ASSERT_TRUE(lib.Load(dll.string()));
        auto probe = reinterpret_cast<ProbeFn>(lib.GetSymbol("dse_test_plugin_probe"));
        ASSERT_NE(probe, nullptr);
        EXPECT_EQ(probe(), 2);
    }

    fs::remove(dll, ec);
}

TEST(NativePluginReload, MissingSymbolReturnsNull) {
    const fs::path dll = fs::temp_directory_path() / "dse_native_plugin_missing_sym.dll";
    std::error_code ec;
    SwapPluginOnDisk(DSE_TEST_PLUGIN_V1_DLL, dll);

    DynamicLibrary lib;
    ASSERT_TRUE(lib.Load(dll.string()));
    EXPECT_EQ(lib.GetSymbol("dse_symbol_that_does_not_exist"), nullptr);
    lib.Unload();
    fs::remove(dll, ec);
}

TEST(NativePluginReload, GetSymbolOnUnloadedLibraryReturnsNull) {
    DynamicLibrary lib;
    EXPECT_FALSE(lib.IsLoaded());
    EXPECT_EQ(lib.GetSymbol("dse_test_plugin_probe"), nullptr);
}
