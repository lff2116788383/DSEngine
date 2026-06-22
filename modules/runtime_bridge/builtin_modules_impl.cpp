/**
 * @file builtin_modules_impl.cpp
 * @brief IBuiltinModules 具体实现 + 工厂函数
 */

#include "modules/runtime_bridge/builtin_modules_impl.h"
#include "engine/audio/audio_system.h"

// ---- Factory ----
std::unique_ptr<IBuiltinModules> CreateBuiltinModules() {
    return std::make_unique<BuiltinModulesImpl>();
}

// ============================================================
// Gameplay2D
// ============================================================

bool BuiltinModulesImpl::InitGameplay2D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) {
    return gameplay2d_module_.OnInit(world, rhi, asset_mgr);
}

void BuiltinModulesImpl::UpdateGameplay2D(World& world, float dt) {
    gameplay2d_module_.OnUpdate(world, dt);
}

void BuiltinModulesImpl::FixedUpdateGameplay2D(World& world, float dt) {
    gameplay2d_module_.OnFixedUpdate(world, dt);
}

void BuiltinModulesImpl::ShutdownGameplay2D(World& world) {
    gameplay2d_module_.OnShutdown(world);
}

void BuiltinModulesImpl::RenderScene2D(World& world, CommandBuffer& cmd) {
    gameplay2d_module_.RenderScene2D(world, cmd);
}

void BuiltinModulesImpl::RenderUI2D(World& world, CommandBuffer& cmd, int w, int h, const glm::mat4& clip) {
    gameplay2d_module_.RenderUI2D(world, cmd, w, h, clip);
}

dse::gameplay2d::AudioSystem& BuiltinModulesImpl::GetAudioSystem() {
    return gameplay2d_module_.audio_system();
}

// ============================================================
// MeshRenderSystem
// ============================================================

void BuiltinModulesImpl::InitMeshSystem(AssetManager* asset_mgr) {
    mesh_render_system_.SetAssetManager(asset_mgr);
}

void BuiltinModulesImpl::ShutdownMeshSystem() {
    mesh_render_system_.SetAssetManager(nullptr);
}

void BuiltinModulesImpl::RenderMeshes(World& world, CommandBuffer& cmd, RhiDevice& device, MeshRenderer& renderer) {
    // 阶段4-M4：注入设备 + 常驻 MeshRenderer，Render 内部经 DrawBatch 取代旧 cmd.DrawMeshBatch。
    mesh_render_system_.SetRenderContext(&device, &renderer);
    mesh_render_system_.Render(world, cmd);
}

void BuiltinModulesImpl::BuildRenderQueues(World& world, dse::render::RenderScene& scene) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.BuildRenderQueues(world, scene);
#else
    mesh_render_system_.BuildRenderQueues(world, scene);
#endif
}

int BuiltinModulesImpl::PrepareGPUScene(World& world, dse::render::RenderPassContext& ctx) {
#ifdef DSE_ENABLE_3D
    return gameplay3d_module_.mesh_render_system().PrepareGPUScene(world, ctx);
#else
    return mesh_render_system_.PrepareGPUScene(world, ctx);
#endif
}

void BuiltinModulesImpl::ResetGPUSceneState() {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.mesh_render_system().ResetGPUDrivenState();
#endif
    mesh_render_system_.ResetGPUDrivenState();
}

const std::vector<dse::gameplay3d::HiZAABB>& BuiltinModulesImpl::CachedAABBs() const {
#ifdef DSE_ENABLE_3D
    return gameplay3d_module_.mesh_render_system().cached_aabbs();
#else
    return mesh_render_system_.cached_aabbs();
#endif
}

int BuiltinModulesImpl::CachedAABBCount() const {
#ifdef DSE_ENABLE_3D
    return gameplay3d_module_.mesh_render_system().cached_aabb_count();
#else
    return mesh_render_system_.cached_aabb_count();
#endif
}

void BuiltinModulesImpl::SetHiZVisibility(const std::vector<uint32_t>& visibility) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.mesh_render_system().SetHiZVisibility(visibility);
#else
    mesh_render_system_.SetHiZVisibility(visibility);
#endif
}

void BuiltinModulesImpl::CleanupGPUResources(RhiDevice* rhi) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.mesh_render_system().CleanupGPUResources(rhi);
#else
    mesh_render_system_.CleanupGPUResources(rhi);
#endif
}

// ============================================================
// Gameplay3D (full build)
// ============================================================

bool BuiltinModulesImpl::InitGameplay3D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) {
#ifdef DSE_ENABLE_3D
    return gameplay3d_module_.OnInit(world, rhi, asset_mgr);
#else
    (void)world; (void)rhi; (void)asset_mgr;
    return false;
#endif
}

void BuiltinModulesImpl::UpdateGameplay3D(World& world, float dt) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.OnUpdate(world, dt);
#else
    (void)world; (void)dt;
#endif
}

void BuiltinModulesImpl::FixedUpdateGameplay3D(World& world, float dt) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.OnFixedUpdate(world, dt);
#else
    (void)world; (void)dt;
#endif
}

void BuiltinModulesImpl::ShutdownGameplay3D(World& world) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.OnShutdown(world);
#else
    (void)world;
#endif
}

dse::core::IModule* BuiltinModulesImpl::GetGameplay3DModule() {
#ifdef DSE_ENABLE_3D
    return &gameplay3d_module_;
#else
    return nullptr;
#endif
}

void BuiltinModulesImpl::RegisterGameplay3DPasses(
    dse::render::RenderGraph& graph,
    dse::render::RenderPassContext& ctx,
    std::vector<std::unique_ptr<dse::render::IRenderPass>>& out_passes) {
#ifdef DSE_ENABLE_3D
    gameplay3d_module_.RegisterRenderPasses(graph, ctx, out_passes);
#else
    (void)graph; (void)ctx; (void)out_passes;
#endif
}

// ============================================================
// Fallback 3D (minimal path when DSE_ENABLE_3D=OFF)
// ============================================================

void BuiltinModulesImpl::InitFallback3D(World& world, RhiDevice* rhi, AssetManager* asset_mgr) {
#ifndef DSE_ENABLE_3D
    particle3d_system_.SetAssetManager(asset_mgr);
    particle3d_system_.Init(world, rhi);
    dse::gameplay3d::AnimatorSystem::SetAssetManager(asset_mgr);
#else
    (void)world; (void)rhi; (void)asset_mgr;
#endif
}

void BuiltinModulesImpl::UpdateFallback3D(World& world, float dt) {
#ifndef DSE_ENABLE_3D
    particle3d_system_.Update(world, dt);
    steering_system_.Update(world, dt);
    dse::gameplay3d::AnimatorSystem::Update(world, dt);
#else
    (void)world; (void)dt;
#endif
}

void BuiltinModulesImpl::ShutdownFallback3D(World& world) {
#ifndef DSE_ENABLE_3D
    particle3d_system_.Shutdown(world);
    particle3d_system_.SetAssetManager(nullptr);
    dse::gameplay3d::AnimatorSystem::SetAssetManager(nullptr);
#else
    (void)world;
#endif
}
