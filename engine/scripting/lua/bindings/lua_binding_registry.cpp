/**
 * @file lua_binding_registry.cpp
 * @brief Lua 脚本绑定与运行时管理，处理 C++ 与 Lua 的交互边界
 */

#include "engine/scripting/lua/bindings/lua_binding_registry.h"
#include "engine/scripting/lua/bindings/lua_binding_modules.h"
#include "engine/scripting/lua/bindings/lua_binding_context.h"
#include "engine/scripting/lua/bindings/lua_binding_free_functions.gen.h"

namespace dse::runtime::lua_binding {

void RegisterPhase1LuaApi(lua_State* L) {
    lua_newtable(L);
    lua_setglobal(L, "dse");

    RegisterContextBindings(L);

    lua_getglobal(L, "dse");

    // --- ECS (self-managing: gets/creates dse.ecs internally) ---
    RegisterEcsBindings(L);

    // --- Manual modules (push table, need lua_setfield) ---
    RegisterAssetsBindings(L);
    lua_setfield(L, -2, "assets");

    RegisterAppBindings(L);
    lua_setfield(L, -2, "app");

    RegisterMetricsBindings(L);
    lua_setfield(L, -2, "metrics");

    RegisterFloatingOriginBindings(L);
    lua_setfield(L, -2, "origin");

    RegisterSerializeBindings(L);
    lua_setfield(L, -2, "serialize");

#ifdef DSE_NET_ENABLED
    // dse.repl 复制层（server/client/RPC）：实现完整但此前注册链从未调用，
    // 导致 LUA_API.md §17.5 文档化 API 运行时不可达。随网络模块一并激活。
    // 注意：RegisterReplBindings 仅把模块表压栈（不做 setfield），由调用方挂到 dse 下。
    RegisterReplBindings(L);
    lua_setfield(L, -2, "repl");
#endif

    // 2D 网格寻路 — dse.pathfinding
    RegisterGridPathfindingBindings(L);
    lua_setfield(L, -2, "pathfinding");

    // 增强版瓦片地图 — dse.tilemap
    RegisterTilemapBindings(L);
    lua_setfield(L, -2, "tilemap");

    lua_setglobal(L, "dse");

    // --- Codegen modules (self-managing stack via lua_getglobal/lua_pop) ---
    RegisterAudioBindings(L);
    RegisterSpineBindings(L);
    RegisterUiBindings(L);
    RegisterLocalizationBindings(L);
    RegisterFontBindings(L);

#ifdef DSE_ENABLE_HTTP
    RegisterHttpBindings(L);
    RegisterHttpRequestBinding(L);
#endif

#ifdef DSE_NET_ENABLED
    RegisterNetBindings(L);
#endif

    // DSSL 材质系统 — 独立全局表 "dssl"
    RegisterDSSLBindings(L);
    // Create standalone "dssl" global alias (backward compat)
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "dssl");
    lua_setglobal(L, "dssl");
    // Create standalone "l10n" global alias
    lua_getglobal(L, "dse");
    lua_getfield(L, -1, "l10n");
    lua_setglobal(L, "l10n");
    lua_pop(L, 1);

    lua_pop(L, 1);

#ifdef DSE_ENABLE_NAVMESH
    // NavMesh 寻路系统 — 独立全局表 "nav" + ecs 扩展
    RegisterNavigationBindings(L);
#endif

    // 资源流式加载 — 独立全局表 "streaming"
    RegisterStreamingBindings(L);

    // Open-world systems — codegen (self-managing)
    RegisterOpenWorldBindings(L);

    // AI 行为树 + GOAP 规划器 — 独立全局表 "ai"
    RegisterAIBindings(L);

    // 过场/导演系统 — 独立全局表 "cutscene"
    RegisterCutsceneBindings(L);

    // Meshlet/Cluster 渲染系统 — 独立全局表 "meshlet"
    RegisterMeshletBindings(L);

    // P2-P5 大世界系统 — codegen (self-managing)
    RegisterOpenWorldP2P5Bindings(L);

    // 6大世界系统（Spline / Ocean / EditorTools / VSM / EQS / Distribution）
    RegisterFreeWorldSplineBindings(L);
    RegisterFreeWorldOceanBindings(L);
    RegisterFreeWorldEditorBindings(L);
    RegisterFreeWorldVsmBindings(L);
    RegisterFreeWorldEqsBindings(L);
    RegisterFreeWorldDistBindings(L);

    // 自由函数 Lua 绑定（codegen 自动生成）
    RegisterAllFreeFunctionBindings(L);
}

}
