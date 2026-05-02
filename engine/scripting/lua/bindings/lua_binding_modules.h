/**
 * @file lua_binding_modules.h
 * @brief Lua 绑定模块注册函数声明
 */

#ifndef DSE_LUA_BINDING_MODULES_H
#define DSE_LUA_BINDING_MODULES_H

extern "C" {
#include "depends/lua/lua.h"
}

namespace dse::runtime::lua_binding {

// 顶层模块注册
void RegisterEcsBindings(lua_State* L);
void RegisterAudioBindings(lua_State* L);
void RegisterSpineBindings(lua_State* L);
void RegisterUiBindings(lua_State* L);
void RegisterAssetsBindings(lua_State* L);
void RegisterAppBindings(lua_State* L);
void RegisterMetricsBindings(lua_State* L);

// ECS 子域注册（由 RegisterEcsBindings 内部调用，栈顶需为 ecs 表）
void RegisterEcsCoreBindings(lua_State* L);
void RegisterEcsTransformBindings(lua_State* L);
void RegisterEcsRenderingBindings(lua_State* L);
void RegisterEcsPhysics2DBindings(lua_State* L);
void RegisterEcsPhysics3DBindings(lua_State* L);
void RegisterEcsAnimationBindings(lua_State* L);
void RegisterEcsParticlesBindings(lua_State* L);

}

#endif
