#pragma once

#include "engine/core/module.h"

// 3D Systems
#if defined(DSE_ENABLE_PHYSX) && defined(DSE_HAS_PHYSX_LIBS)
#include "engine/physics/physics3d/physics3d_system.h"
#endif
#include "modules/gameplay_3d/rendering/mesh_render_system.h"
#include "modules/gameplay_3d/rendering/terrain_system.h"
#include "modules/gameplay_3d/rendering/frustum_culling_system.h"
#include "modules/gameplay_3d/animation/animator_system.h"
#include "modules/gameplay_3d/particles/particle3d_system.h"
#include "modules/gameplay_3d/camera/free_camera_controller_system.h"
#include "modules/gameplay_3d/ai/steering_system.h"

namespace dse {
namespace gameplay3d {

/**
 * @class Gameplay3DModule
 * @brief 将所有的 3D 功能系统打包为一个独立模块，实现与引擎核心的解耦
 */
class Gameplay3DModule : public core::IModule {
public:
    const char* GetName() const override { return "Gameplay3D"; }

    bool OnInit(World& world, RhiDevice* rhi_device, AssetManager* asset_manager) override {
        if (asset_manager == nullptr) {
            return false;
        }
#if defined(DSE_ENABLE_PHYSX) && defined(DSE_HAS_PHYSX_LIBS)
        physics3d_system_.Init(world);
#endif
        mesh_render_system_.SetAssetManager(asset_manager);
        animator_system_.SetAssetManager(asset_manager);
        particle3d_system_.SetAssetManager(asset_manager);
        particle3d_system_.Init(world, rhi_device);
        return true;
    }

    void OnUpdate(World& world, float delta_time) override {
        free_camera_controller_system_.Update(world, delta_time);
        animator_system_.Update(world, delta_time);
        particle3d_system_.Update(world, delta_time);
        steering_system_.Update(world, delta_time);
        frustum_culling_system_.Update(world);
    }

    void OnFixedUpdate(World& world, float fixed_delta_time) override {
#if defined(DSE_ENABLE_PHYSX) && defined(DSE_HAS_PHYSX_LIBS)
        physics3d_system_.FixedUpdate(world, fixed_delta_time);
#else
        (void)world;
        (void)fixed_delta_time;
#endif
    }

    void OnRenderPreZ(World& world, CommandBuffer& cmd_buffer) override {
        terrain_system_.Render(world, cmd_buffer);
        mesh_render_system_.Render(world, cmd_buffer);
    }

    void OnRenderShadow(World& world, CommandBuffer& cmd_buffer, int cascade_index, const glm::mat4& light_view, const glm::mat4& light_proj) override {
        terrain_system_.Render(world, cmd_buffer);
        mesh_render_system_.Render(world, cmd_buffer);
    }

    void OnRenderScene(World& world, CommandBuffer& cmd_buffer) override {
        terrain_system_.Render(world, cmd_buffer);
        mesh_render_system_.Render(world, cmd_buffer);
        
        // Note: Since particle uses a specific shader inside RhiDevice (or if we use a state),
        // We just submit them here. The RHI will handle the pipeline switch internally for DrawParticles3D
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
        
        // We need the active camera to draw particles correctly.
        // As a simple module implementation, we find the active camera here.
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
            glm::mat4 projection = glm::perspective(glm::radians(camera.fov),
                                                    16.0f / 9.0f, // Simple fallback ratio
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

    void OnShutdown(World& world) override {
#if defined(DSE_ENABLE_PHYSX) && defined(DSE_HAS_PHYSX_LIBS)
        physics3d_system_.Shutdown();
#endif
        particle3d_system_.Shutdown(world);
        mesh_render_system_.SetAssetManager(nullptr);
        animator_system_.SetAssetManager(nullptr);
        particle3d_system_.SetAssetManager(nullptr);
    }

private:
#if defined(DSE_ENABLE_PHYSX) && defined(DSE_HAS_PHYSX_LIBS)
    physics3d::Physics3DSystem physics3d_system_;
#endif
    MeshRenderSystem mesh_render_system_;
    TerrainSystem terrain_system_;
    FrustumCullingSystem frustum_culling_system_;
    AnimatorSystem animator_system_;
    Particle3DSystem particle3d_system_;
    FreeCameraControllerSystem free_camera_controller_system_;
    SteeringSystem steering_system_;
};

} // namespace gameplay3d
} // namespace dse

