#include "modules/gameplay_3d/gameplay_3d_module.h"
#include "engine/render/render_scene.h"
#include "engine/core/service_locator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/scene/scene_manager.h"
#include "engine/ai/behavior_tree.h"
#include "engine/cutscene/cutscene_player.h"
#include "engine/ecs/parallel_each.h"

namespace dse::gameplay3d {

bool Gameplay3DModule::OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) {
    if (asset_manager == nullptr) {
        return false;
    }
#if defined(DSE_ENABLE_PHYSX) && defined(DSE_HAS_PHYSX_LIBS)
    // PhysX is owned by the engine-side FramePipeline so Lua bindings and runtime systems
    // share the same ServiceLocator instance. The dynamic Gameplay3D DLL must not create
    // a second PxFoundation in its own module-local locator.
    (void)world;
#endif
    rhi_device_ = rhi_device;
    terrain_system_.Init(rhi_device);
    grass_system_.Init(rhi_device);
    tree_system_.Init(rhi_device);
    tree_system_.SetAssetManager(asset_manager);
    hair_system_.Init(rhi_device);
    mesh_render_system_.SetAssetManager(asset_manager);
    lod_system_.SetAssetManager(asset_manager);
    animator_system_.SetAssetManager(asset_manager);
    anim_layer_blend_system_.SetAssetManager(asset_manager);
    particle3d_system_.SetAssetManager(asset_manager);
    particle3d_system_.Init(world, rhi_device);
    cloth_system_.SetAssetManager(asset_manager);
    fluid_system_.Init(world, rhi_device);
    softbody_system_.SetAssetManager(asset_manager);
#ifdef DSE_HAS_PHYSICS3D
    fracture_system_.SetAssetManager(asset_manager);
    auto* physics3d = dse::core::ServiceLocator::Instance().Get<dse::physics3d::IPhysics3DSystem>();
    fracture_system_.SetPhysics3D(physics3d);
    ragdoll_system_.SetAssetManager(asset_manager);
    ragdoll_system_.SetPhysics3D(physics3d);
    vehicle_system_.SetPhysics3D(physics3d);
    buoyancy_system_.SetPhysics3D(physics3d);
#endif

    // ── Open-world systems 初始化 ──────────────────────────────────────────

    // World Partition: 通过 ServiceLocator 获取 SceneManager 依赖
    auto* scene_mgr = dse::core::ServiceLocator::Instance().Get<::scene::SceneManager>();
    world_partition_system_.Init(scene_mgr);

    // HLOD: 检查场景根是否有 HLODConfigComponent
    {
        auto hlod_v = world.registry().view<dse::render::HLODConfigComponent>();
        for (auto e : hlod_v) {
            auto& cfg = hlod_v.get<dse::render::HLODConfigComponent>(e);
            if (cfg.enabled && !cfg.hlod_data_path.empty()) {
                hlod_system_.Init(world, cfg.hlod_data_path);
                break;
            }
        }
    }

    // Virtual Texture: 检查场景中是否有 VirtualTextureComponent
    {
        auto vt_v = world.registry().view<dse::vt::VirtualTextureComponent>();
        for (auto e : vt_v) {
            auto& vtc = vt_v.get<dse::vt::VirtualTextureComponent>(e);
            if (vtc.enabled) {
                dse::vt::VirtualTextureConfig vt_config;
                vt_config.virtual_size = vtc.virtual_width;
                vt_config.tile_data_path = vtc.tile_data_path;
                virtual_texture_system_.Init(vt_config, rhi_device);
                break;
            }
        }
    }

    // Geometry Clipmap: 默认配置初始化
    {
        dse::terrain::GeometryClipmapConfig clipmap_config;
        geometry_clipmap_system_.Init(clipmap_config);
    }

    // Global SDF: 默认配置初始化
    {
        dse::render::GlobalSDFConfig sdf_config;
        global_sdf_system_.Init(sdf_config);
    }

    // AI LOD Scheduler: 默认配置初始化
    {
        dse::ai::AILodConfig ai_config;
        ai_lod_scheduler_.Init(ai_config);
    }

    // GPU Particle Manager: 初始化 compute shader
    gpu_particle_manager_.Init(rhi_device);

    // Lightmap System: 初始化（加载 .dlightmap → GPU 纹理）
    lightmap_system_.Init(rhi_device);

    // World State Persistence: 初始化存档目录
    world_state_persistence_.Init("save/world_state");

    // 注册到 ServiceLocator（Lua 绑定通过此访问系统，no-op deleter 因为是成员变量）
    {
        auto noop_del = [](void*) {};
        auto& sl = dse::core::ServiceLocator::Instance();
        sl.Register<dse::WorldPartitionSystem, dse::WorldPartitionSystem>(
            std::shared_ptr<dse::WorldPartitionSystem>(&world_partition_system_, noop_del));
        sl.Register<dse::render::HLODSystem, dse::render::HLODSystem>(
            std::shared_ptr<dse::render::HLODSystem>(&hlod_system_, noop_del));
        sl.Register<dse::vt::VirtualTextureSystem, dse::vt::VirtualTextureSystem>(
            std::shared_ptr<dse::vt::VirtualTextureSystem>(&virtual_texture_system_, noop_del));
        sl.Register<dse::terrain::GeometryClipmapSystem, dse::terrain::GeometryClipmapSystem>(
            std::shared_ptr<dse::terrain::GeometryClipmapSystem>(&geometry_clipmap_system_, noop_del));
        sl.Register<dse::render::GlobalSDFSystem, dse::render::GlobalSDFSystem>(
            std::shared_ptr<dse::render::GlobalSDFSystem>(&global_sdf_system_, noop_del));
        sl.Register<dse::ai::AILodScheduler, dse::ai::AILodScheduler>(
            std::shared_ptr<dse::ai::AILodScheduler>(&ai_lod_scheduler_, noop_del));
        sl.Register<dse::WorldStatePersistence, dse::WorldStatePersistence>(
            std::shared_ptr<dse::WorldStatePersistence>(&world_state_persistence_, noop_del));
        sl.Register<dse::cutscene::CutscenePlayer, dse::cutscene::CutscenePlayer>(
            std::shared_ptr<dse::cutscene::CutscenePlayer>(&cutscene_player_, noop_del));
    }

    // Floating Origin: 订阅 rebase 事件，偏移各子系统持有的世界空间坐标
    world_cache_ = &world;
    auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>();
    if (event_bus) {
        origin_rebase_handle_ = event_bus->Subscribe<dse::core::OriginRebasedEvent>(
            [this](const dse::core::OriginRebasedEvent& evt) {
                if (!world_cache_) return;
                auto& reg = world_cache_->registry();

                // Particle3D
                for (auto [e, ps] : reg.view<dse::ParticleSystem3DComponent>().each()) {
                    for (int i = 0; i < ps.active_particle_count; ++i) {
                        ps.particles[i].position -= evt.offset;
                    }
                }

                // Cloth
                for (auto [e, cloth] : reg.view<dse::ClothComponent>().each()) {
                    if (!cloth.initialized) continue;
                    for (auto& p : cloth.positions)      p -= evt.offset;
                    for (auto& p : cloth.prev_positions)  p -= evt.offset;
                }

                // Fluid
                for (auto [e, fluid] : reg.view<dse::FluidEmitterComponent>().each()) {
                    for (uint32_t i = 0; i < fluid.active_count; ++i) {
                        fluid.particles[i].position -= evt.offset;
                    }
                }

                // SoftBody
                for (auto [e, sb] : reg.view<dse::SoftBodyComponent>().each()) {
                    for (auto& p : sb.positions)      p -= evt.offset;
                    for (auto& p : sb.prev_positions)  p -= evt.offset;
                }

                // Rope
                for (auto [e, rope] : reg.view<dse::RopeComponent>().each()) {
                    if (!rope.initialized) continue;
                    for (auto& p : rope.positions)      p -= evt.offset;
                    for (auto& p : rope.prev_positions)  p -= evt.offset;
                }
            });
    }

    return true;
}

void Gameplay3DModule::OnUpdate(World& world, float delta_time) {
    OnUpdate(world, dse::FrameUpdateContext{dse::TimeContext{delta_time, delta_time, 1.0f}, 0});
}

void Gameplay3DModule::OnUpdate(World& world, const dse::FrameUpdateContext& frame) {
    const dse::TimeContext& time = frame.time;
    const float delta_time = time.scaled_dt;  // gameplay/动画/粒子（逐实体缩放在各系统内部应用）
    // 自由相机控制走真实时间（调试/取景，不随暂停冻结）
    free_camera_controller_system_.Update(world, time.unscaled_dt);

    // Animation LOD: 根据距摄像机距离设置跳帧率
    {
        glm::vec3 cam_pos(0.0f);
        auto cam_v = world.registry().view<dse::Camera3DComponent, TransformComponent>();
        for (auto e : cam_v) {
            auto& cam = cam_v.get<dse::Camera3DComponent>(e);
            if (cam.enabled) {
                cam_pos = cam_v.get<TransformComponent>(e).position;
                break;
            }
        }
        // ParallelEach: Animation LOD —— 每实体独立读写自己的组件，无写冲突
        dse::ecs::ParallelEach<Animator3DComponent, TransformComponent>(world,
            [cam_pos](Entity /*e*/, Animator3DComponent& animator, TransformComponent& tf) {
                float dist_sq = glm::dot(tf.position - cam_pos, tf.position - cam_pos);
                if (dist_sq > 2500.0f)       animator.anim_lod_skip_ = 3;
                else if (dist_sq > 400.0f)   animator.anim_lod_skip_ = 1;
                else                          animator.anim_lod_skip_ = 0;
            }, 128);
    }

    // 昼夜循环（驱动太阳方向 → DirectionalLight）
    day_night_cycle_system_.Update(world, delta_time);

    // Animation pipeline: EvaluateBaseAnim → LayerBlend → IK → FootIK → ComputeFinalMatrices
    animator_system_.EvaluateBaseAnim(world, delta_time);
    anim_layer_blend_system_.Update(world, delta_time);
    ik_solver_system_.Update(world, delta_time);
    foot_ik_system_.Update(world, delta_time);
    animator_system_.ComputeFinalMatrices(world);
    BoneAttachmentSystem::Update(world);
    weather_system_.Update(world, delta_time);
    snow_cover_system_.Update(world, delta_time);
    particle3d_system_.Update(world, delta_time);
    steering_system_.Update(world, delta_time);

    // ── Behavior Tree: tick all entities with BehaviorTreeComponent (parallel) ──
    dse::ecs::ParallelEach<dse::BehaviorTreeComponent>(world,
        [delta_time](Entity /*e*/, dse::BehaviorTreeComponent& btc) {
            if (!btc.enabled || !btc.tree) return;
            auto status = btc.tree->Tick(delta_time);
            if (status != dse::ai::BTStatus::Running && btc.auto_restart) {
                btc.tree->Reset();
            }
        }, 32);

    // ── Cutscene Player: tick the global cutscene player ──
    cutscene_player_.Update(delta_time);

    // Auto-play CutsceneComponents that haven't started yet
    {
        auto cs_view = world.registry().view<dse::CutsceneComponent>();
        for (auto e : cs_view) {
            auto& csc = cs_view.get<dse::CutsceneComponent>(e);
            if (!csc.enabled) continue;
            if (csc.auto_play && !csc.playing) {
                if (cutscene_player_.GetSequence(csc.sequence_name)) {
                    cutscene_player_.Play(csc.sequence_name);
                    csc.playing = true;
                    csc.auto_play = false;
                }
            }
        }
    }

#ifdef DSE_ENABLE_NAVMESH
    nav_agent_system_.Update(world, delta_time);
#endif
#ifdef DSE_HAS_PHYSICS3D
    fracture_system_.Update(world, delta_time);
#endif
    fluid_system_.Update(world, delta_time);
    grass_system_.Update(world, delta_time);
    tree_system_.Update(world, delta_time);

    // Hair: extract camera pos for LOD
    {
        glm::vec3 cam_pos(0.0f);
        auto cam_v = world.registry().view<dse::Camera3DComponent, TransformComponent>();
        for (auto e : cam_v) {
            auto& cam = cam_v.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            auto& ct = cam_v.get<TransformComponent>(e);
            cam_pos = glm::vec3(ct.local_to_world * glm::vec4(0, 0, 0, 1));
            break;
        }
        hair_system_.Update(world, cam_pos, delta_time);
    }

    // ── Lightmap: 按需加载 LightmapComponent 引用的 .dlightmap 纹理 ──
    lightmap_system_.Update(world);

    // ── Open-world systems 更新 ──────────────────────────────────────────

    // World Partition: 基于 StreamingOriginComponent 异步加载/卸载 Cell
    world_partition_system_.Update(world);

    // Virtual Texture: feedback 驱动流式页加载
    virtual_texture_system_.Update(++frame_number_);

    // Geometry Clipmap: 基于相机位置更新地形 LOD
    {
        glm::vec3 clipmap_cam_pos(0.0f);
        auto clipmap_cam_v = world.registry().view<dse::Camera3DComponent, TransformComponent>();
        for (auto e : clipmap_cam_v) {
            auto& cam = clipmap_cam_v.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            clipmap_cam_pos = clipmap_cam_v.get<TransformComponent>(e).position;
            break;
        }
        geometry_clipmap_system_.Update(clipmap_cam_pos);
        global_sdf_system_.Update(clipmap_cam_pos);
    }

    // GPU Particles: compute dispatch for each GpuParticleComponent
    if (rhi_device_) {
        auto gpu_p_view = world.registry().view<dse::render::GpuParticleComponent, TransformComponent>();
        for (auto e : gpu_p_view) {
            auto& comp = gpu_p_view.get<dse::render::GpuParticleComponent>(e);
            if (!comp.config.enabled) continue;
            if (!comp.initialized) {
                gpu_particle_manager_.InitComponent(comp, rhi_device_);
            }
            auto& tf = gpu_p_view.get<TransformComponent>(e);
            gpu_particle_manager_.Update(comp, rhi_device_, tf.position, delta_time);
        }
    }

    frustum_culling_system_.Update(world);
    lod_system_.Update(world);

    // HLOD: 在 LOD 之后更新，基于相机距离切换 proxy
    {
        glm::vec3 hlod_cam_pos(0.0f);
        auto hlod_cam_v = world.registry().view<dse::Camera3DComponent, TransformComponent>();
        for (auto e : hlod_cam_v) {
            auto& cam = hlod_cam_v.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            hlod_cam_pos = hlod_cam_v.get<TransformComponent>(e).position;
            break;
        }
        hlod_system_.Update(world, hlod_cam_pos);
    }

    // Impostor: 在 LOD 之后更新，利用同一帧相机位置收集远景实例
    if (rhi_device_) {
        glm::vec3 imp_cam_pos(0.0f);
        auto imp_cam_v = world.registry().view<dse::Camera3DComponent, TransformComponent>();
        for (auto e : imp_cam_v) {
            auto& cam = imp_cam_v.get<dse::Camera3DComponent>(e);
            if (!cam.enabled) continue;
            imp_cam_pos = glm::vec3(imp_cam_v.get<TransformComponent>(e).local_to_world[3]);
            break;
        }
        impostor_system_.SetRenderContext(rhi_device_, glm::mat4(1.0f), glm::mat4(1.0f),
                                          imp_cam_pos, glm::vec3(0, -1, 0), glm::vec3(0.15f));
        impostor_system_.Update(world, imp_cam_pos, *rhi_device_);
    }
    mesh_render_system_.MarkBatchDirty();
}

void Gameplay3DModule::OnFixedUpdate(World& world, float fixed_delta_time) {
    cloth_system_.FixedUpdate(world, fixed_delta_time);
    softbody_system_.FixedUpdate(world, fixed_delta_time);
    rope_system_.FixedUpdate(world, fixed_delta_time);
#ifdef DSE_HAS_PHYSICS3D
    ragdoll_system_.FixedUpdate(world, fixed_delta_time);
    vehicle_system_.FixedUpdate(world, fixed_delta_time);
    buoyancy_system_.FixedUpdate(world, fixed_delta_time);
#endif
}

void Gameplay3DModule::BuildRenderQueues(World& world, dse::render::RenderScene& scene) {
    mesh_render_system_.BuildRenderQueues(world, scene);
    // 各渲染阶段（prez/shadow/opaque）的贡献统一通过 ISceneRenderer 注册，
    // 由内建 PreZ / Shadow / Forward / RSM Pass 在各自渲染作用域内按阶段调用。
    scene.scene_renderers.push_back(this);
    // Impostor LOD: 远景 billboard 作为独立 ISceneRenderer 贡献 Opaque 阶段
    scene.scene_renderers.push_back(impostor_system_.AsSceneRenderer());
}

void Gameplay3DModule::RenderPreZ(dse::render::CommandBuffer& cmd,
                                  const dse::render::RenderScenePassContext& ctx) {
    if (!ctx.world) return;
    dse::render::FrameContext frame;
    if (ctx.view) frame.view = *ctx.view;
    if (ctx.projection) frame.projection = *ctx.projection;
    // PreZ 深度预通道：绑定无彩色深度 RT → 走 DrawMeshBatch 深度路径（depth_only=true）。
    terrain_system_.Render(*ctx.world, cmd, frame, ctx.camera_offset, /*depth_only=*/true);
    grass_system_.Render(*ctx.world, cmd, frame, ctx.camera_offset, /*depth_only=*/true);
    tree_system_.Render(*ctx.world, cmd, frame, ctx.camera_offset, /*depth_only=*/true);
}

void Gameplay3DModule::RenderShadow(dse::render::CommandBuffer& cmd,
                                    const dse::render::RenderScenePassContext& ctx) {
    if (!ctx.world) return;
    dse::render::FrameContext frame;
    if (ctx.view) frame.view = *ctx.view;
    if (ctx.projection) frame.projection = *ctx.projection;
    // 阴影 pass：深度 RT（depth_only=true）；地形无独立阴影方法，复用 Render 深度路径。
    terrain_system_.Render(*ctx.world, cmd, frame, ctx.camera_offset, /*depth_only=*/true);
    grass_system_.RenderShadow(*ctx.world, cmd, frame, ctx.camera_offset);
    tree_system_.RenderShadow(*ctx.world, cmd, frame, ctx.camera_offset);
}

void Gameplay3DModule::RenderOpaque(dse::render::CommandBuffer& cmd,
                                    const dse::render::RenderScenePassContext& ctx) {
    if (!ctx.world) return;
    World& callback_world = *ctx.world;
    dse::render::FrameContext frame;
    if (ctx.view) frame.view = *ctx.view;
    if (ctx.projection) frame.projection = *ctx.projection;
    // Opaque 彩色通道：彩色 RT（depth_only=false）→ 走 MeshRenderer 前向路径。
    terrain_system_.Render(callback_world, cmd, frame, ctx.camera_offset, /*depth_only=*/false);
    grass_system_.Render(callback_world, cmd, frame, ctx.camera_offset, /*depth_only=*/false);
    tree_system_.Render(callback_world, cmd, frame, ctx.camera_offset, /*depth_only=*/false);

    auto p_view = callback_world.registry().view<dse::ParticleSystem3DComponent>();
    std::vector<dse::render::ParticleDrawItem> p_items;
    for (auto entity : p_view) {
        const auto& ps = p_view.get<dse::ParticleSystem3DComponent>(entity);
        if (ps.enabled && ps.active_particle_count > 0 && ps.instance_vbo != 0) {
            dse::render::ParticleDrawItem item;
            item.texture_handle = ps.texture_handle;
            item.particle_count = ps.active_particle_count;
            item.instance_buffer = ps.instance_vbo;
            p_items.push_back(item);
        }
    }

    auto f_view = callback_world.registry().view<dse::FluidEmitterComponent>();
    for (auto entity : f_view) {
        const auto& fluid = f_view.get<dse::FluidEmitterComponent>(entity);
        if (fluid.enabled && fluid.active_count > 0 && fluid.instance_vbo != 0) {
            dse::render::ParticleDrawItem item;
            item.texture_handle = 0;
            item.particle_count = static_cast<int>(fluid.active_count);
            item.instance_buffer = fluid.instance_vbo;
            p_items.push_back(item);
        }
    }

    const glm::mat4 view_at_origin = ctx.view ? *ctx.view : glm::mat4(1.0f);
    const glm::mat4 projection = ctx.projection ? *ctx.projection : glm::mat4(1.0f);
    // Camera-Relative: 粒子/毛发数据仍在世界空间，需要用包含 camera_offset 平移的 view
    const glm::mat4 world_to_view = view_at_origin * glm::translate(glm::mat4(1.0f), -ctx.camera_offset);
    if (!p_items.empty() && rhi_device_ != nullptr) {
        // B3：高层 ParticleRenderer 走通用绘制原语 + BuiltinProgram::Particle3D（取代 DrawParticles3D ABI）。
        particle_renderer_.DrawParticles(cmd, *rhi_device_, p_items, world_to_view, projection);
    }
    hair_system_.Render(callback_world, cmd, world_to_view, projection);
}

void Gameplay3DModule::OnShutdown(World& world) {
    // Floating Origin: 取消订阅
    if (origin_rebase_handle_.valid) {
        auto* event_bus = dse::core::ServiceLocator::Instance().Get<dse::core::EventBus>();
        if (event_bus) event_bus->Unsubscribe(origin_rebase_handle_);
        origin_rebase_handle_ = {};
    }
    world_cache_ = nullptr;

    // Cutscene Player: 停止并清空
    cutscene_player_.Stop();
    cutscene_player_.ClearTriggers();

    // Open-world systems: ServiceLocator 注销
    {
        auto& sl = dse::core::ServiceLocator::Instance();
        sl.Reset<dse::cutscene::CutscenePlayer>();
        sl.Reset<dse::WorldPartitionSystem>();
        sl.Reset<dse::render::HLODSystem>();
        sl.Reset<dse::vt::VirtualTextureSystem>();
        sl.Reset<dse::terrain::GeometryClipmapSystem>();
        sl.Reset<dse::render::GlobalSDFSystem>();
        sl.Reset<dse::ai::AILodScheduler>();
        sl.Reset<dse::WorldStatePersistence>();
    }

    // Lightmap system shutdown
    lightmap_system_.Shutdown();

    // Open-world systems shutdown
    world_partition_system_.Shutdown();
    hlod_system_.Shutdown(world);
    virtual_texture_system_.Shutdown();
    geometry_clipmap_system_.Shutdown();
    global_sdf_system_.Shutdown();
    ai_lod_scheduler_.Shutdown();
    if (rhi_device_ != nullptr) {
        // GPU Particle: 释放各组件 GPU 资源
        auto gpu_p_view = world.registry().view<dse::render::GpuParticleComponent>();
        for (auto e : gpu_p_view) {
            auto& comp = gpu_p_view.get<dse::render::GpuParticleComponent>(e);
            gpu_particle_manager_.ShutdownComponent(comp, rhi_device_);
        }
        gpu_particle_manager_.Shutdown(rhi_device_);
    }
    world_state_persistence_.SaveAll();

    terrain_system_.Shutdown(world);
    grass_system_.Shutdown(world);
    tree_system_.Shutdown(world);
    hair_system_.Shutdown(world);
    weather_system_.Shutdown(world);
    snow_cover_system_.Shutdown(world);
    particle3d_system_.Shutdown(world);
    if (rhi_device_ != nullptr) particle_renderer_.Shutdown(*rhi_device_);
    if (rhi_device_ != nullptr) impostor_system_.Shutdown(*rhi_device_);
    mesh_render_system_.SetAssetManager(nullptr);
    lod_system_.SetAssetManager(nullptr);
    animator_system_.SetAssetManager(nullptr);
    anim_layer_blend_system_.SetAssetManager(nullptr);
    particle3d_system_.SetAssetManager(nullptr);
    cloth_system_.SetAssetManager(nullptr);
    fluid_system_.Shutdown(world);
    softbody_system_.SetAssetManager(nullptr);
#ifdef DSE_HAS_PHYSICS3D
    fracture_system_.SetAssetManager(nullptr);
    fracture_system_.SetPhysics3D(nullptr);
    ragdoll_system_.SetAssetManager(nullptr);
    ragdoll_system_.SetPhysics3D(nullptr);
    vehicle_system_.SetPhysics3D(nullptr);
    buoyancy_system_.SetPhysics3D(nullptr);
#endif
}

} // namespace dse::gameplay3d

// Gameplay3DModule 已静态编入 dse_engine，FramePipeline 直接持有实例，
// 不再需要 DLL 工厂函数 CreateModule/DestroyModule。
