#include "modules/gameplay_3d/gameplay_3d_module.h"

#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef DSE_GAMEPLAY3D_EXPORTS
#define DSE_MODULE_API __declspec(dllexport)
#else
#define DSE_MODULE_API __declspec(dllimport)
#endif
#else
#define DSE_MODULE_API __attribute__((visibility("default")))
#endif

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
    animator_system_.SetAssetManager(asset_manager);
    particle3d_system_.SetAssetManager(asset_manager);
    particle3d_system_.Init(world, rhi_device);
    return true;
}

void Gameplay3DModule::OnUpdate(World& world, float delta_time) {
    free_camera_controller_system_.Update(world, delta_time);
    animator_system_.Update(world, delta_time);
    particle3d_system_.Update(world, delta_time);
    steering_system_.Update(world, delta_time);
    frustum_culling_system_.Update(world);
}

void Gameplay3DModule::OnFixedUpdate(World& world, float fixed_delta_time) {
    (void)world;
    (void)fixed_delta_time;
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

void Gameplay3DModule::OnRenderScene(World& world, CommandBuffer& cmd_buffer) {
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
    animator_system_.SetAssetManager(nullptr);
    particle3d_system_.SetAssetManager(nullptr);
}

} // namespace dse::gameplay3d

extern "C" {

DSE_MODULE_API dse::core::IModule* CreateModule() {
    return new dse::gameplay3d::Gameplay3DModule();
}

DSE_MODULE_API void DestroyModule(dse::core::IModule* module) {
    delete module;
}

}
