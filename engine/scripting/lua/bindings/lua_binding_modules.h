/**
 * @file lua_binding_modules.h
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#ifndef DSE_LUA_BINDING_MODULES_H
#define DSE_LUA_BINDING_MODULES_H

extern "C" {
#include <lua.h>
}

namespace dse::runtime::lua_binding {

void RegisterEcsBindings(lua_State* L);
void RegisterAudioBindings(lua_State* L);
void RegisterUiBindings(lua_State* L);
void RegisterAssetsBindings(lua_State* L);
void RegisterAppBindings(lua_State* L);
void RegisterMetricsBindings(lua_State* L);

}

#endif
