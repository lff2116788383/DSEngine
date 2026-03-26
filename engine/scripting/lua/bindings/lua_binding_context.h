/**
 * @file lua_binding_context.h
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

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
void RegisterContextBindings(lua_State* L);
const LuaApiContext& GetBindingContext();
World* GetWorld();
AssetManager& GetAssetManager();
Entity LuaEntityFromInteger(lua_Integer value);

}

#endif
