/*
 * Native plugin hot-reload test fixture.
 *
 * Built twice (DSE_TEST_PLUGIN_VERSION=1 and =2) into two DLLs so an integration
 * test can prove dse::core::DynamicLibrary can load a plugin, unload it, swap the
 * DLL on disk, and reload it with the new behavior taking effect — the exact path
 * apps/editor_cpp/src/editor_plugin_hot_reload.cpp depends on. [P2-1]
 */

#if defined(_WIN32)
#define DSE_PLUGIN_EXPORT __declspec(dllexport)
#else
#define DSE_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#ifndef DSE_TEST_PLUGIN_VERSION
#define DSE_TEST_PLUGIN_VERSION 0
#endif

DSE_PLUGIN_EXPORT int dse_test_plugin_probe(void) {
    return DSE_TEST_PLUGIN_VERSION;
}
