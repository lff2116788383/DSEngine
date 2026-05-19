/**
 * @file lua_binding_modules.h
 * @brief Lua 绑定模块注册函数声明
 */

#ifndef DSE_LUA_BINDING_MODULES_H
#define DSE_LUA_BINDING_MODULES_H

extern "C" {
#include "depends/lua/lua.h"
}

#include "engine/scripting/lua/bindings/lua_binding_helper.h"

namespace dse::runtime::lua_binding {

// 顶层模块注册
void RegisterEcsBindings(lua_State* L);
void RegisterAudioBindings(lua_State* L);
void RegisterSpineBindings(lua_State* L);
void RegisterUiBindings(lua_State* L);
void RegisterAssetsBindings(lua_State* L);
void RegisterAppBindings(lua_State* L);
void RegisterMetricsBindings(lua_State* L);
void RegisterDSSLBindings(lua_State* L);
#ifdef DSE_ENABLE_NAVMESH
void RegisterNavigationBindings(lua_State* L);
#endif
void RegisterStreamingBindings(lua_State* L);

// ECS 子域注册（由 RegisterEcsBindings 内部调用，栈顶需为 ecs 表）
void RegisterEcsCoreBindings(lua_State* L);
void RegisterEcsRenderingBindings(lua_State* L);

// Codegen 生成的组件属性绑定（来自 lua_binding_ecs_*.gen.cpp）
void RegisterTransformComponentGenBindings(lua_State* L);
void RegisterCamera3DComponentGenBindings(lua_State* L);
void RegisterDirectionalLight3DComponentGenBindings(lua_State* L);
void RegisterPointLightComponentGenBindings(lua_State* L);
void RegisterMeshRendererComponentGenBindings(lua_State* L);
void RegisterEcsPhysics2DBindings(lua_State* L);
void RegisterEcsPhysics3DBindings(lua_State* L);
void RegisterEcsAnimationBindings(lua_State* L);
void RegisterEcsParticlesBindings(lua_State* L);
void RegisterEcsGameplay3DBindings(lua_State* L);

}

#endif
