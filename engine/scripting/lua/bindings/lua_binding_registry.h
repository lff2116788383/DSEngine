#ifndef DSE_LUA_BINDING_REGISTRY_H
#define DSE_LUA_BINDING_REGISTRY_H

extern "C" {
#include <lua.h>
}

namespace dse::runtime::lua_binding {

void RegisterPhase1LuaApi(lua_State* L);

}

#endif
