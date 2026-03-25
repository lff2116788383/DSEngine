#ifndef DSE_LUA_BINDING_CONTEXT_H
#define DSE_LUA_BINDING_CONTEXT_H

#include "engine/scripting/lua/lua_runtime.h"
#include "engine/ecs/world.h"
extern "C" {
#include <lua.h>
}

class AssetManager;

namespace dse::runtime::lua_binding {

void ConfigureBindingContext(const LuaApiContext& context);
const LuaApiContext& GetBindingContext();
World* GetWorld();
AssetManager& GetAssetManager();
Entity LuaEntityFromInteger(lua_Integer value);

}

#endif
