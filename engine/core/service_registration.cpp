/**
 * @file service_registration.cpp
 * @brief Registers engine singletons into ServiceLocator with non-owning shared_ptr.
 */
#include "engine/core/service_registration.h"
#include "engine/core/service_locator.h"
#ifdef DSE_ENABLE_HTTP
#include "engine/http/http_client.h"
#endif
#include "engine/render/material/dssl_material_loader.h"
#include "engine/scripting/lua/lua_debugger.h"

namespace dse {
namespace core {

void RegisterEngineSingletons() {
    auto& sl = ServiceLocator::Instance();

    // Non-owning shared_ptr wrapping existing singletons (no-op deleter)
#ifdef DSE_ENABLE_HTTP
    auto http_ptr = std::shared_ptr<http::HttpClient>(
        &http::HttpClient::Instance(), [](http::HttpClient*) {});
    sl.Register<http::HttpClient, http::HttpClient>(http_ptr);
#endif

    auto dssl_ptr = std::shared_ptr<render::DSSLMaterialLoader>(
        &render::DSSLMaterialLoader::Instance(), [](render::DSSLMaterialLoader*) {});
    sl.Register<render::DSSLMaterialLoader, render::DSSLMaterialLoader>(dssl_ptr);

    auto dbg_ptr = std::shared_ptr<scripting::LuaDebugger>(
        &scripting::LuaDebugger::Instance(), [](scripting::LuaDebugger*) {});
    sl.Register<scripting::LuaDebugger, scripting::LuaDebugger>(dbg_ptr);
}

void UnregisterEngineSingletons() {
    auto& sl = ServiceLocator::Instance();
#ifdef DSE_ENABLE_HTTP
    sl.Reset<http::HttpClient>();
#endif
    sl.Reset<render::DSSLMaterialLoader>();
    sl.Reset<scripting::LuaDebugger>();
}

} // namespace core
} // namespace dse
