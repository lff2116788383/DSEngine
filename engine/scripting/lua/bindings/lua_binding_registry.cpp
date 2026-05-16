/**
 * @file lua_binding_registry.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"

namespace dse::runtime::lua_binding {

void RegisterPhase1LuaApi(lua_State* L) {
    lua_newtable(L);
    lua_setglobal(L, "dse");

    RegisterContextBindings(L);

    lua_getglobal(L, "dse");

    RegisterEcsBindings(L);
    lua_setfield(L, -2, "ecs");

    RegisterAudioBindings(L);
    lua_setfield(L, -2, "audio");

    RegisterSpineBindings(L);
    lua_setfield(L, -2, "spine");

    RegisterUiBindings(L);
    lua_setfield(L, -2, "ui");

    RegisterAssetsBindings(L);
    lua_setfield(L, -2, "assets");

    RegisterAppBindings(L);
    lua_setfield(L, -2, "app");

    RegisterMetricsBindings(L);
    lua_setfield(L, -2, "metrics");

    lua_setglobal(L, "dse");

    // DSSL 材质系统 — 独立全局表 "dssl"
    RegisterDSSLBindings(L);

#ifdef DSE_ENABLE_NAVMESH
    // NavMesh 寻路系统 — 独立全局表 "nav" + ecs 扩展
    RegisterNavigationBindings(L);
#endif

    // 资源流式加载 — 独立全局表 "streaming"
    RegisterStreamingBindings(L);
}

}
