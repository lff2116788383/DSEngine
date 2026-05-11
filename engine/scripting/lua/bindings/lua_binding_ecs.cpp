/**
 * @file lua_binding_ecs.cpp
 * @brief ECS Lua 绑定分发器 — 聚合各域子模块注册函数
 *
 * 原 2289 行单体文件已拆分为 7 个域文件：
 * - lua_binding_ecs_core.cpp        实体创建、场景加载、查询
 * - lua_binding_ecs_transform.cpp   TransformComponent
 * - lua_binding_ecs_rendering.cpp   Camera/Sprite/Mesh/Light/Skybox/Terrain/PostProcess/Steering
 * - lua_binding_ecs_physics2d.cpp   2D 物理 + Tilemap
 * - lua_binding_ecs_physics3d.cpp   3D 物理 + Raycast
 * - lua_binding_ecs_animation.cpp   2D/3D 动画 + FSM
 * - lua_binding_ecs_particles.cpp   粒子系统 + GameplayTuning
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"
extern "C" {
#include "depends/lua/lua.h"
}

namespace dse::runtime::lua_binding {

void RegisterEcsBindings(lua_State* L) {
    lua_newtable(L);

    RegisterEcsCoreBindings(L);
    RegisterEcsTransformBindings(L);
    RegisterEcsRenderingBindings(L);
    RegisterEcsPhysics2DBindings(L);
    RegisterEcsPhysics3DBindings(L);
    RegisterEcsAnimationBindings(L);
    RegisterEcsParticlesBindings(L);
    RegisterEcsGameplay3DBindings(L);
}

} // namespace dse::runtime::lua_binding
