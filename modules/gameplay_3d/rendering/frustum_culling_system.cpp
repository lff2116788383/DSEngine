#include "frustum_culling_system.h"
#include "engine/ecs/components_3d.h"
#include <glm/gtc/matrix_transform.hpp>

namespace dse {
namespace gameplay3d {

void FrustumCullingSystem::Update(World& world) {
    // 1. Find main camera
    auto camera_view = world.registry().view<Camera3DComponent>();
    entt::entity main_camera = entt::null;
    int max_priority = -9999;

    for (auto entity : camera_view) {
        auto& cam = camera_view.get<Camera3DComponent>((entt::entity)entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_camera = (entt::entity)entity;
        }
    }

    if (main_camera == entt::null) {
        visible_set_.Clear();
        return;
    }

    auto& cam = camera_view.get<Camera3DComponent>((entt::entity)main_camera);
    glm::mat4 projection = glm::perspective(glm::radians(cam.fov), cam.aspect_ratio, cam.near_clip, cam.far_clip);
    glm::mat4 view = glm::mat4(1.0f);
    if (world.registry().all_of<TransformComponent>((entt::entity)main_camera)) {
        auto& transform = world.registry().get<TransformComponent>((entt::entity)main_camera);
        glm::vec3 front = transform.rotation * glm::vec3(0.0f, 0.0f, -1.0f);
        glm::vec3 up = transform.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        view = glm::lookAt(transform.position, transform.position + front, up);
    }

    glm::mat4 view_proj = projection * view;

    // 2. Lazy-build 静态 Octree（首次或 Invalidate 后重建）
    if (!spatial_scene_.IsBuilt()) {
        spatial_scene_.BuildStatic(world);
    }

    // 3. 更新动态物体包围盒
    spatial_scene_.UpdateDynamicBounds(world);

    // 4. 执行剔除，输出分层可见集
    spatial_scene_.CullFrustum(view_proj, world, visible_set_);
}

} // namespace gameplay3d
} // namespace dse
