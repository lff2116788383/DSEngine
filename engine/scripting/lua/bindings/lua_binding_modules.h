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
void RegisterLocalizationBindings(lua_State* L);
void RegisterFloatingOriginBindings(lua_State* L);
void RegisterFontBindings(lua_State* L);
void RegisterSerializeBindings(lua_State* L);  // dse.serialize 自描述二进制序列化（编解码 Lua 值/表）
#ifdef DSE_ENABLE_HTTP
void RegisterHttpBindings(lua_State* L);   // dse.http 异步 HTTP(S) 客户端
void PumpHttp(lua_State* L);               // 触发已完成 HTTP 回调（引擎 Tick 调用）
#endif
#ifdef DSE_NET_ENABLED
void RegisterNetBindings(lua_State* L);    // dse.net 游戏网络传输 (GameNetworkingSockets)
void PumpNet(lua_State* L);                // 每帧泵：派发网络事件回调（引擎 Tick 调用）
void RegisterReplBindings(lua_State* L);   // dse.repl 复制层 + RPC (服务器/客户端)
#endif

// ECS 子域注册（由 RegisterEcsBindings 内部调用，栈顶需为 ecs 表）
void RegisterEcsCoreBindings(lua_State* L);
void RegisterEcsRenderingBindings(lua_State* L);
// S1.8：渲染绑定按域拆分（由 RegisterEcsRenderingBindings 聚合调用）
void RegisterEcsRenderingCameraBindings(lua_State* L);
void RegisterEcsRenderingMeshBindings(lua_State* L);
void RegisterEcsRenderingLightBindings(lua_State* L);
void RegisterEcsRenderingPostBindings(lua_State* L);
void RegisterEcsRenderingTerrainBindings(lua_State* L);
void RegisterEcsRenderingFxBindings(lua_State* L);

// Codegen 生成的组件属性绑定（来自 lua_binding_ecs_*.gen.cpp）
void RegisterTransformComponentGenBindings(lua_State* L);
void RegisterCamera3DComponentGenBindings(lua_State* L);
void RegisterDirectionalLight3DComponentGenBindings(lua_State* L);
void RegisterPointLightComponentGenBindings(lua_State* L);
void RegisterMeshRendererComponentGenBindings(lua_State* L);
void RegisterSpotLightComponentGenBindings(lua_State* L);
void RegisterSkyLightComponentGenBindings(lua_State* L);
void RegisterTreeComponentGenBindings(lua_State* L);
void RegisterTerrainTileManagerComponentGenBindings(lua_State* L);
void RegisterDynamicObstacleComponentGenBindings(lua_State* L);
void RegisterNavMeshAutoRebakeComponentGenBindings(lua_State* L);
void RegisterPostProcessComponentGenBindings(lua_State* L);
void RegisterAnimator3DComponentGenBindings(lua_State* L);
void RegisterEcsPhysics2DBindings(lua_State* L);
void RegisterEcsPhysics3DBindings(lua_State* L);
void RegisterEcsAnimationBindings(lua_State* L);
void RegisterEcsParticlesBindings(lua_State* L);
void RegisterEcsGameplay3DBindings(lua_State* L);
void Register2DSystemsBindings(lua_State* L);  // Parallax/Light2D/Trail/Line/CameraCtrl/AudioSpatial/SpriteSheet/Atlas

// Open-world systems (world_partition, hlod, vt, clipmap, sdf, ai_lod, gpu_particles, world_state, procedural)
void RegisterOpenWorldBindings(lua_State* L);

// AI 行为树 + GOAP 规划器 — 独立全局表 "ai"
void RegisterAIBindings(lua_State* L);

// 过场/导演系统 — 独立全局表 "cutscene"
void RegisterCutsceneBindings(lua_State* L);

// Meshlet/Cluster 渲染系统 — 独立全局表 "meshlet"
void RegisterMeshletBindings(lua_State* L);

// P2-P5 大世界系统（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）
void RegisterOpenWorldP2P5Bindings(lua_State* L);
void ShutdownOpenWorldP2P5Bindings();

// 6大世界系统（Spline / Ocean / EditorTools / VSM / EQS / Distribution）
void RegisterWorldSystemsBindings(lua_State* L);
void ShutdownWorldSystemsBindings();

}

#endif
