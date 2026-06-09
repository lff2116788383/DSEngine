/**
 * @file lua_binding_ecs_rendering.cpp
 * @brief ECS Lua 绑定 — 渲染相关（Camera、Sprite、MeshRenderer、Light、Skybox、
 *        Terrain、PostProcess、Steering、FreeCameraController）
 */

#include "engine/scripting/lua/bindings/lua_binding_modules.h"

namespace dse::runtime::lua_binding {

// S1.8：本文件已按域拆分为 lua_binding_ecs_rendering_{camera,mesh,light,post,terrain,fx}.cpp；
// 此处仅保留聚合入口，依次调用各域注册函数（行为与拆分前完全一致）。
void RegisterEcsRenderingBindings(lua_State* L) {
    RegisterEcsRenderingCameraBindings(L);
    RegisterEcsRenderingMeshBindings(L);
    RegisterEcsRenderingLightBindings(L);
    RegisterEcsRenderingPostBindings(L);
    RegisterEcsRenderingTerrainBindings(L);
    RegisterEcsRenderingFxBindings(L);
}

} // namespace dse::runtime::lua_binding
