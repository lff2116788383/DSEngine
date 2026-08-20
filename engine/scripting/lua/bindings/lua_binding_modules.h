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

#include <vector>
#include <functional>

namespace dse::runtime::lua_binding {

/// Centralized cleanup registry for Lua binding modules.
/// Each binding with static state registers its cleanup callback here
/// so LuaRuntime::Shutdown() can clear all state in one pass.
class BindingCleanupRegistry {
public:
    static BindingCleanupRegistry& Instance() {
        static BindingCleanupRegistry inst;
        return inst;
    }
    void Register(std::function<void()> fn) { callbacks_.push_back(std::move(fn)); }
    void RunAll() { for (auto& fn : callbacks_) fn(); }
    void Clear()  { callbacks_.clear(); }
private:
    BindingCleanupRegistry() = default;
    std::vector<std::function<void()>> callbacks_;
};

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
void RegisterGridPathfindingBindings(lua_State* L);  // dse.pathfinding 2D 网格寻路
void RegisterStreamingBindings(lua_State* L);
void RegisterLocalizationBindings(lua_State* L);
void RegisterFloatingOriginBindings(lua_State* L);
void RegisterFontBindings(lua_State* L);
void RegisterSerializeBindings(lua_State* L);  // dse.serialize 自描述二进制序列化（编解码 Lua 值/表）
void RegisterTilemapBindings(lua_State* L);  // dse.tilemap 增强版瓦片地图（多层/动画/属性/序列化）
#ifdef DSE_ENABLE_HTTP
void RegisterHttpBindings(lua_State* L);        // dse.http 低层 C ABI 绑定（codegen 生成）
void RegisterHttpRequestBinding(lua_State* L);  // dse.http.request{...on_done} 高层回调式请求（手写）
void PumpHttp(lua_State* L);                     // 触发已完成 HTTP 回调（引擎 Tick 调用）
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
// 新增组件由 codegen 自动生成，此处声明对应的注册函数。
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
void RegisterDecalComponentGenBindings(lua_State* L);
void RegisterSkyboxComponentGenBindings(lua_State* L);
void RegisterFreeCameraControllerComponentGenBindings(lua_State* L);
void RegisterSubSceneComponentGenBindings(lua_State* L);
void RegisterBoundingBoxComponentGenBindings(lua_State* L);
void RegisterWaterComponentGenBindings(lua_State* L);
void RegisterLightProbeComponentGenBindings(lua_State* L);
void RegisterReflectionProbeComponentGenBindings(lua_State* L);
void RegisterGIProbeVolumeComponentGenBindings(lua_State* L);
void RegisterFoliageComponentGenBindings(lua_State* L);
void RegisterAtmosphereComponentGenBindings(lua_State* L);
void RegisterVolumetricCloudComponentGenBindings(lua_State* L);
void RegisterDayNightCycleComponentGenBindings(lua_State* L);
void RegisterHairComponentGenBindings(lua_State* L);
void RegisterImpostorComponentGenBindings(lua_State* L);
void RegisterStreamingOriginComponentGenBindings(lua_State* L);
void RegisterWorldPartitionConfigComponentGenBindings(lua_State* L);
void RegisterHLODConfigComponentGenBindings(lua_State* L);
void RegisterVirtualTextureComponentGenBindings(lua_State* L);
void RegisterLightmapComponentGenBindings(lua_State* L);
void RegisterRigidBody3DComponentGenBindings(lua_State* L);
void RegisterBoxCollider3DComponentGenBindings(lua_State* L);
void RegisterSphereCollider3DComponentGenBindings(lua_State* L);
void RegisterCapsuleCollider3DComponentGenBindings(lua_State* L);
void RegisterMeshCollider3DComponentGenBindings(lua_State* L);
void RegisterCharacterController3DComponentGenBindings(lua_State* L);
void RegisterJoint3DComponentGenBindings(lua_State* L);
void RegisterRagdollComponentGenBindings(lua_State* L);
void RegisterSoftBodyComponentGenBindings(lua_State* L);
void RegisterVehicleComponentGenBindings(lua_State* L);
void RegisterRopeComponentGenBindings(lua_State* L);
void RegisterBuoyancyComponentGenBindings(lua_State* L);
void RegisterCharacterMovementStateGenBindings(lua_State* L);
void RegisterCharacterMovementConfigGenBindings(lua_State* L);
void RegisterSpringArm3DComponentGenBindings(lua_State* L);
void RegisterPlayerControllerComponentGenBindings(lua_State* L);
void RegisterJiggleBoneComponentGenBindings(lua_State* L);
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
void ShutdownAIBindings();

// 过场/导演系统 — 独立全局表 "cutscene"
void RegisterCutsceneBindings(lua_State* L);
void ShutdownCutsceneBindings();

// Meshlet/Cluster 渲染系统 — 独立全局表 "meshlet"
void RegisterMeshletBindings(lua_State* L);
void ShutdownMeshletBindings();

// 视频播放系统 — 独立全局表 "dse.video"
void ShutdownVideoBindings();

// P2-P5 大世界系统（Mesh Streaming / Physics LOD / Terrain Deform / Audio LOD）
void RegisterOpenWorldP2P5Bindings(lua_State* L);
void ShutdownOpenWorldP2P5Bindings();

// 6大世界系统（Spline / Ocean / EditorTools / VSM / EQS / Distribution）— codegen 生成
void RegisterFreeWorldSplineBindings(lua_State* L);
void RegisterFreeWorldOceanBindings(lua_State* L);
void RegisterFreeWorldEditorBindings(lua_State* L);
void RegisterFreeWorldVsmBindings(lua_State* L);
void RegisterFreeWorldEqsBindings(lua_State* L);
void RegisterFreeWorldDistBindings(lua_State* L);

// C ABI 差距修补 — 补齐手写 C ABI 中尚未映射到 Lua 的函数


// Gap-fill register functions (codegen)
void RegisterFreeGapBindings(lua_State* L);
void RegisterFreeFn_api_core(lua_State* L);
void RegisterFreeFn_app(lua_State* L);
void RegisterFreeFn_audio(lua_State* L);
void RegisterFreeFn_ecs_gap(lua_State* L);
void RegisterFreeFn_localization(lua_State* L);
void RegisterFreeFn_ui(lua_State* L);

}

#endif
