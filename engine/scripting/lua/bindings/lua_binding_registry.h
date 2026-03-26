/**
 * @file lua_binding_registry.h
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#ifndef DSE_LUA_BINDING_REGISTRY_H
#define DSE_LUA_BINDING_REGISTRY_H

extern "C" {
#include <lua.h>
}

namespace dse::runtime::lua_binding {

void RegisterPhase1LuaApi(lua_State* L);

}

#endif
