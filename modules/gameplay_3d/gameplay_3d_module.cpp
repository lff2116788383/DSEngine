#include "modules/gameplay_3d/gameplay_3d_module.h"
#include "engine/core/service_locator.h"
#ifdef DSE_ENABLE_PHYSX
#include "engine/physics/physics3d/physics3d_system.h"
#endif
#include "engine/ecs/components_3d_fluid.h"

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
    mesh_render_system_.SetAssetManager(asset_manager);
    lod_system_.SetAssetManager(asset_manager);
    animator_system_.SetAssetManager(asset_manager);
    anim_layer_blend_system_.SetAssetManager(asset_manager);
    particle3d_system_.SetAssetManager(asset_manager);
    particle3d_system_.Init(world, rhi_device);
    cloth_system_.SetAssetManager(asset_manager);
    fluid_system_.Init(world, rhi_device);
    softbody_system_.SetAssetManager(asset_manager);
#ifdef DSE_ENABLE_PHYSX
    fracture_system_.SetAssetManager(asset_manager);
    ragdoll_system_.SetAssetManager(asset_manager);
#endif
#if defined(DSE_ENABLE_PHYSX)
    auto* physics3d = dse::core::ServiceLocator::Instance().Get<dse::physics3d::Physics3DSystem>();
    fracture_system_.SetPhysics3D(physics3d);
    ragdoll_system_.SetPhysics3D(physics3d);
    vehicle_system_.SetPhysics3D(physics3d);
    buoyancy_system_.SetPhysics3D(physics3d);
#endif
    return true;
}

void Gameplay3DModule::OnUpdate(World& world, float delta_time) {
    free_camera_controller_system_.Update(world, delta_time);
    // Animation pipeline: EvaluateBaseAnim → LayerBlend → IK → ComputeFinalMatrices
    animator_system_.EvaluateBaseAnim(world, delta_time);
    anim_layer_blend_system_.Update(world, delta_time);
    ik_solver_system_.Update(world, delta_time);
    animator_system_.ComputeFinalMatrices(world);
    particle3d_system_.Update(world, delta_time);
    steering_system_.Update(world, delta_time);
#ifdef DSE_ENABLE_NAVMESH
    nav_agent_system_.Update(world, delta_time);
#endif
#ifdef DSE_ENABLE_PHYSX
    fracture_system_.Update(world, delta_time);
#endif
    fluid_system_.Update(world, delta_time);
    frustum_culling_system_.Update(world);
    lod_system_.Update(world);
}

void Gameplay3DModule::OnFixedUpdate(World& world, float fixed_delta_time) {
    cloth_system_.FixedUpdate(world, fixed_delta_time);
    softbody_system_.FixedUpdate(world, fixed_delta_time);
    rope_system_.FixedUpdate(world, fixed_delta_time);
#ifdef DSE_ENABLE_PHYSX
    ragdoll_system_.FixedUpdate(world, fixed_delta_time);
    vehicle_system_.FixedUpdate(world, fixed_delta_time);
    buoyancy_system_.FixedUpdate(world, fixed_delta_time);
#endif
}

void Gameplay3DModule::OnRenderPreZ(World& world, CommandBuffer& cmd_buffer) {
    terrain_system_.Render(world, cmd_buffer);
    mesh_render_system_.Render(world, cmd_buffer);
}

void Gameplay3DModule::OnRenderShadow(World& world, CommandBuffer& cmd_buffer, int cascade_index, const glm::mat4& light_view, const glm::mat4& light_proj) {
    (void)cascade_index;
    (void)light_view;
    (void)light_proj;
    terrain_system_.Render(world, cmd_buffer);
    mesh_render_system_.Render(world, cmd_buffer);
}

void Gameplay3DModule::OnRenderScene(World& world, CommandBuffer& cmd_buffer, const glm::mat4& clip_correction) {
    terrain_system_.Render(world, cmd_buffer);
    mesh_render_system_.Render(world, cmd_buffer);

    auto p_view = world.registry().view<dse::ParticleSystem3DComponent>();
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

    // Collect fluid emitter particles (same instance format as Particle3D)
    auto f_view = world.registry().view<dse::FluidEmitterComponent>();
    for (auto entity : f_view) {
        const auto& fluid = f_view.get<dse::FluidEmitterComponent>(entity);
        if (fluid.enabled && fluid.active_count > 0 && fluid.instance_vbo != 0) {
            Particle3DDrawItem item;
            item.texture_handle = 0; // No texture, use color from instance data
            item.particle_count = static_cast<int>(fluid.active_count);
            item.instance_vbo = fluid.instance_vbo;
            p_items.push_back(item);
        }
    }

    auto camera3d_view = world.registry().view<dse::Camera3DComponent>();
    entt::entity selected_camera3d = entt::null;
    int selected_priority3d = std::numeric_limits<int>::min();
    for (auto entity : camera3d_view) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(entity);
        if (camera.enabled && camera.priority > selected_priority3d) {
            selected_camera3d = entity;
            selected_priority3d = camera.priority;
        }
    }

    if (!p_items.empty() && selected_camera3d != entt::null) {
        auto& camera = camera3d_view.get<dse::Camera3DComponent>(selected_camera3d);
        glm::mat4 projection = clip_correction * glm::perspective(glm::radians(camera.fov),
                                                16.0f / 9.0f,
                                                camera.near_clip, camera.far_clip);
        glm::mat4 view = glm::mat4(1.0f);
        if (world.registry().all_of<TransformComponent>(selected_camera3d)) {
            auto& transform = world.registry().get<TransformComponent>(selected_camera3d);
            glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
            view = glm::lookAt(transform.position, transform.position + front, up);
        }
        cmd_buffer.DrawParticles3D(p_items, view, projection);
    }
}

void Gameplay3DModule::OnShutdown(World& world) {
    particle3d_system_.Shutdown(world);
    mesh_render_system_.SetAssetManager(nullptr);
    lod_system_.SetAssetManager(nullptr);
    animator_system_.SetAssetManager(nullptr);
    anim_layer_blend_system_.SetAssetManager(nullptr);
    particle3d_system_.SetAssetManager(nullptr);
    cloth_system_.SetAssetManager(nullptr);
    fluid_system_.Shutdown(world);
    softbody_system_.SetAssetManager(nullptr);
#ifdef DSE_ENABLE_PHYSX
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
