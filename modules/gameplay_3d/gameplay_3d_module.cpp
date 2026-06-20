#include "modules/gameplay_3d/gameplay_3d_module.h"
#include "engine/render/render_scene.h"
#include "engine/core/service_locator.h"
#include <glm/gtc/matrix_transform.hpp>
#include <limits>
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/components_3d_fluid.h"
#include "engine/ecs/components_3d_physics.h"

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
    free_camera_controller_system_.Update(world, delta_time);

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
        auto anim_v = world.registry().view<Animator3DComponent, TransformComponent>();
        for (auto e : anim_v) {
            auto& animator = anim_v.get<Animator3DComponent>(e);
            const auto& tf = anim_v.get<TransformComponent>(e);
            float dist_sq = glm::dot(tf.position - cam_pos, tf.position - cam_pos);
            if (dist_sq > 2500.0f)       animator.anim_lod_skip_ = 3; // >50m: 每4帧
            else if (dist_sq > 400.0f)   animator.anim_lod_skip_ = 1; // >20m: 每2帧
            else                          animator.anim_lod_skip_ = 0; // <=20m: 每帧
        }
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

    frustum_culling_system_.Update(world);
    lod_system_.Update(world);
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
}

void Gameplay3DModule::RenderPreZ(dse::render::CommandBuffer& cmd,
                                  const dse::render::RenderScenePassContext& ctx) {
    if (!ctx.world) return;
    // PreZ 深度预通道：绑定无彩色深度 RT → 走 DrawMeshBatch 深度路径（depth_only=true）。
    terrain_system_.Render(*ctx.world, cmd, ctx.camera_offset, /*depth_only=*/true);
    grass_system_.Render(*ctx.world, cmd, ctx.camera_offset, /*depth_only=*/true);
    tree_system_.Render(*ctx.world, cmd, ctx.camera_offset, /*depth_only=*/true);
}

void Gameplay3DModule::RenderShadow(dse::render::CommandBuffer& cmd,
                                    const dse::render::RenderScenePassContext& ctx) {
    if (!ctx.world) return;
    // 阴影 pass：深度 RT（depth_only=true）；地形无独立阴影方法，复用 Render 深度路径。
    terrain_system_.Render(*ctx.world, cmd, ctx.camera_offset, /*depth_only=*/true);
    grass_system_.RenderShadow(*ctx.world, cmd, ctx.camera_offset);
    tree_system_.RenderShadow(*ctx.world, cmd, ctx.camera_offset);
}

void Gameplay3DModule::RenderOpaque(dse::render::CommandBuffer& cmd,
                                    const dse::render::RenderScenePassContext& ctx) {
    if (!ctx.world) return;
    World& callback_world = *ctx.world;
    // Opaque 彩色通道：彩色 RT（depth_only=false）→ 走 MeshRenderer 前向路径。
    terrain_system_.Render(callback_world, cmd, ctx.camera_offset, /*depth_only=*/false);
    grass_system_.Render(callback_world, cmd, ctx.camera_offset, /*depth_only=*/false);
    tree_system_.Render(callback_world, cmd, ctx.camera_offset, /*depth_only=*/false);

    auto p_view = callback_world.registry().view<dse::ParticleSystem3DComponent>();
    std::vector<Particle3DDrawItem> p_items;
    for (auto entity : p_view) {
        const auto& ps = p_view.get<dse::ParticleSystem3DComponent>(entity);
        if (ps.enabled && ps.active_particle_count > 0 && ps.instance_vbo != 0) {
            Particle3DDrawItem item;
            item.texture_handle = ps.texture_handle;
            item.particle_count = ps.active_particle_count;
            item.instance_vbo = ps.instance_vbo;
            p_items.push_back(item);
        }
    }

    auto f_view = callback_world.registry().view<dse::FluidEmitterComponent>();
    for (auto entity : f_view) {
        const auto& fluid = f_view.get<dse::FluidEmitterComponent>(entity);
        if (fluid.enabled && fluid.active_count > 0 && fluid.instance_vbo != 0) {
            Particle3DDrawItem item;
            item.texture_handle = 0;
            item.particle_count = static_cast<int>(fluid.active_count);
            item.instance_vbo = fluid.instance_vbo;
            p_items.push_back(item);
        }
    }

    const glm::mat4 view_at_origin = ctx.view ? *ctx.view : glm::mat4(1.0f);
    const glm::mat4 projection = ctx.projection ? *ctx.projection : glm::mat4(1.0f);
    // Camera-Relative: 粒子/毛发数据仍在世界空间，需要用包含 camera_offset 平移的 view
    const glm::mat4 world_to_view = view_at_origin * glm::translate(glm::mat4(1.0f), -ctx.camera_offset);
    if (!p_items.empty()) {
        cmd.DrawParticles3D(p_items, world_to_view, projection);
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

    terrain_system_.Shutdown(world);
    grass_system_.Shutdown(world);
    tree_system_.Shutdown(world);
    hair_system_.Shutdown(world);
    weather_system_.Shutdown(world);
    snow_cover_system_.Shutdown(world);
    particle3d_system_.Shutdown(world);
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
