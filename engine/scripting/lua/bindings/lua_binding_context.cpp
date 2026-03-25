#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/assets/asset_manager.h"

namespace dse::runtime::lua_binding {
namespace {
LuaApiContext g_binding_context{};
}

void ConfigureBindingContext(const LuaApiContext& context) {
    g_binding_context = context;
}

const LuaApiContext& GetBindingContext() {
    return g_binding_context;
}

World* GetWorld() {
    return g_binding_context.world;
}

AssetManager& GetAssetManager() {
    if (g_binding_context.asset_manager) {
        return *g_binding_context.asset_manager;
    }
    return AssetManager::Instance();
}

Entity LuaEntityFromInteger(lua_Integer value) {
    return static_cast<Entity>(static_cast<std::uint32_t>(value));
}

}
