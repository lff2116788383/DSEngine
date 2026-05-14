#include "lod_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

namespace dse {
namespace gameplay3d {

void LODSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void LODSystem::Update(World& world) {
    if (!asset_manager_) return;

    auto cam_view = world.registry().view<Camera3DComponent>();
    entt::entity main_camera = entt::null;
    int max_priority = std::numeric_limits<int>::min();
    for (auto entity : cam_view) {
        const auto& cam = cam_view.get<Camera3DComponent>(entity);
        if (cam.enabled && cam.priority > max_priority) {
            max_priority = cam.priority;
            main_camera = entity;
        }
    }
    if (main_camera == entt::null) return;

    const auto& cam = cam_view.get<Camera3DComponent>(main_camera);
    glm::vec3 cam_pos(0.0f);
    if (world.registry().all_of<TransformComponent>(main_camera)) {
        const auto& cam_tf = world.registry().get<TransformComponent>(main_camera);
        cam_pos = glm::vec3(cam_tf.local_to_world[3]);
    }

    const float proj_scale = 1.0f / std::tan(glm::radians(cam.fov) * 0.5f);
    const float proj_scale_sq = proj_scale * proj_scale;

    auto lod_view = world.registry().view<TransformComponent, MeshRendererComponent,
                                          LODGroupComponent>();
    for (auto entity : lod_view) {
        auto& transform     = lod_view.get<TransformComponent>(entity);
        auto& mesh_renderer = lod_view.get<MeshRendererComponent>(entity);
        auto& lod_group     = lod_view.get<LODGroupComponent>(entity);

        // Disabled：恢复原始 mesh_path 并清空缓存，交由 EnsureMeshPathDataLoaded 重载
        if (!lod_group.enabled || lod_group.levels.empty()) {
            if (lod_group.current_lod != -1) {
                if (!lod_group.original_mesh_path.empty()) {
                    mesh_renderer.mesh_path = lod_group.original_mesh_path;
                }
                mesh_renderer.temp_vertices.clear();
                mesh_renderer.temp_indices.clear();
                mesh_renderer.mesh_handle_override = 0;
                lod_group.current_lod = -1;
            }
            continue;
        }

        const glm::vec3 entity_pos = glm::vec3(transform.local_to_world[3]);

        float bbox_radius = 1.0f;
        if (const auto* bbox = world.registry().try_get<BoundingBoxComponent>(entity)) {
            const float r = glm::length(bbox->extents());
            if (r > 0.001f) {
                const glm::mat3 m(transform.local_to_world);
                const float max_scale = std::max({glm::length(m[0]),
                                                  glm::length(m[1]),
                                                  glm::length(m[2])});
                bbox_radius = r * max_scale;
            }
        }

        const glm::vec3 diff = entity_pos - cam_pos;
        const float dist_sq = std::max(1.0f, glm::dot(diff, diff));

        // screen_size = (proj_scale² × bbox_radius²) / max(1, dist²) × global_scale
        const float screen_size = (proj_scale_sq * bbox_radius * bbox_radius)
                                  / dist_sq * lod_group.global_scale;

        // 选第一个 screen_size > threshold 的级别；无匹配则选最低细节
        int target_lod = static_cast<int>(lod_group.levels.size()) - 1;
        for (int i = 0; i < static_cast<int>(lod_group.levels.size()); ++i) {
            if (screen_size > lod_group.levels[i].screen_size_threshold) {
                target_lod = i;
                break;
            }
        }

        // 滞后死区：防止阈值边界抖动导致每帧深拷贝
        const float hyst = lod_group.hysteresis;
        if (hyst > 0.0f && lod_group.current_lod >= 0 && target_lod != lod_group.current_lod) {
            if (target_lod < lod_group.current_lod) {
                // 升级（更高细节）：需要 screen_size 超过阈值上边界才切换
                if (screen_size <= lod_group.levels[target_lod].screen_size_threshold * (1.0f + hyst)) {
                    target_lod = lod_group.current_lod;
                }
            } else {
                // 降级（更低细节）：需要 screen_size 低于当前阈值下边界才切换
                if (screen_size >= lod_group.levels[lod_group.current_lod].screen_size_threshold
                                   * (1.0f - hyst)) {
                    target_lod = lod_group.current_lod;
                }
            }
        }

        if (target_lod == lod_group.current_lod) continue;

        auto& level = lod_group.levels[target_lod];

        // 首次访问该级别：分配稳定 ID 并预热 AssetManager 缓存（消除切换帧的磁盘 I/O）
        if (!level.loaded) {
            asset_manager_->LoadDmesh(level.mesh_path);
            level.mesh_handle = next_handle_++;
            level.loaded = true;
        }

        // 首次 LOD 切换时记录原始 mesh_path，用于 disable 时恢复
        if (lod_group.current_lod == -1 && lod_group.original_mesh_path.empty()) {
            lod_group.original_mesh_path = mesh_renderer.mesh_path;
        }

        // 切换路径并清空 CPU 缓存 → MeshRenderSystem::EnsureMeshPathDataLoaded 在本帧完成解析
        mesh_renderer.mesh_path            = level.mesh_path;
        mesh_renderer.temp_vertices.clear();
        mesh_renderer.temp_indices.clear();
        mesh_renderer.mesh_handle_override = level.mesh_handle;
        lod_group.current_lod              = target_lod;
    }
}

} // namespace gameplay3d
} // namespace dse
