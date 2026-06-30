/**
 * @file impostor_system.cpp
 * @brief ImpostorSystem 实现 — 见头文件说明。
 */

#include "engine/render/impostor/impostor_system.h"

#include "engine/ecs/world.h"
#include "engine/ecs/transform.h"
#include "engine/ecs/components_3d_render.h"
#include "engine/render/render_scene.h"
#include "engine/render/rhi/rhi_device.h"

#include <entt/entt.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>

namespace dse {
namespace render {

namespace {

/// 计算实体包围球半径（从 local_bounds 或默认值）
float GetBoundsRadius(const MeshRendererComponent* mesh_comp) {
    if (!mesh_comp || !mesh_comp->local_bounds_valid) return 1.0f;
    glm::vec3 extent = mesh_comp->local_bounds_max - mesh_comp->local_bounds_min;
    return glm::length(extent) * 0.5f;
}

}  // namespace

void ImpostorSystem::Update(World& world, const glm::vec3& camera_pos, RhiDevice& device) {
    batches_.clear();

    // 按 atlas 纹理句柄分组
    std::unordered_map<unsigned int, size_t> batch_map;  // atlas_handle → batch index

    auto& registry = world.registry();
    auto view = registry.view<ImpostorComponent, TransformComponent>();

    for (auto entity : view) {
        auto& impostor = view.get<ImpostorComponent>(entity);
        if (!impostor.enabled) continue;
        if (!impostor.atlas_loaded_ || impostor.atlas_texture_handle_ == 0) continue;

        // auto_from_lod_group: 从 LODGroupComponent 自动获取切换距离
        float transition_dist = impostor.transition_distance;
        if (impostor.auto_from_lod_group) {
            auto* lod_group = registry.try_get<LODGroupComponent>(entity);
            if (lod_group && !lod_group->levels.empty()) {
                // 使用最后一级 LOD 的 screen_size_threshold 估算距离
                // 当 screen_size < threshold 时进入 impostor
                // 简单估算: 若 LOD 被 cull 了（lod_culled=true），说明已超出 mesh LOD 范围
                if (lod_group->lod_culled) {
                    transition_dist = 0.0f;  // 强制显示 impostor
                }
            }
        }

        auto& transform = view.get<TransformComponent>(entity);
        glm::vec3 world_pos = glm::vec3(transform.local_to_world[3]);

        float distance = glm::length(camera_pos - world_pos);

        // 距离判断
        if (distance < transition_dist) continue;  // 太近，用正常 mesh
        if (distance > impostor.cull_distance) continue;        // 太远，剔除

        // 计算渐变因子
        float fade = 1.0f;
        float fade_start = transition_dist;
        float fade_end = fade_start + impostor.fade_range;
        if (distance < fade_end) {
            fade = (distance - fade_start) / impostor.fade_range;
            fade = std::clamp(fade, 0.0f, 1.0f);
        }

        // 计算相机到物体方向
        glm::vec3 view_dir = glm::normalize(world_pos - camera_pos);

        // 计算 atlas 帧索引
        int frame_x = 0, frame_y = 0;
        ComputeFrameIndex(view_dir, impostor.frame_mode,
                          impostor.frames_x, impostor.frames_y,
                          frame_x, frame_y);

        // 计算 billboard 半尺寸
        float bounds_radius = impostor.cached_bounds_radius_;
        if (bounds_radius <= 0.0f) {
            // 尝试从 MeshRendererComponent 获取
            auto* mesh_comp = registry.try_get<MeshRendererComponent>(entity);
            bounds_radius = GetBoundsRadius(mesh_comp);
            impostor.cached_bounds_radius_ = bounds_radius;
        }
        float half_size = bounds_radius * impostor.impostor_size;

        // 构建实例
        ImpostorDrawInstance inst;
        inst.world_position = world_pos;
        inst.half_size = half_size;
        inst.frame_x = frame_x;
        inst.frame_y = frame_y;
        inst.frames_x_total = impostor.frames_x;
        inst.frames_y_total = impostor.frames_y;
        inst.pivot_offset = impostor.pivot_offset;
        inst.fade = fade;

        // 按 atlas 纹理分批
        unsigned int key = impostor.atlas_texture_handle_;
        auto it = batch_map.find(key);
        if (it == batch_map.end()) {
            batch_map[key] = batches_.size();
            ImpostorBatchItem batch;
            batch.atlas_texture = impostor.atlas_texture_handle_;
            batch.normal_atlas_texture = impostor.normal_texture_handle_;
            batch.normal_strength = impostor.normal_strength;
            batch.alpha_cutoff = 0.5f;
            batch.instances.push_back(inst);
            batches_.push_back(std::move(batch));
        } else {
            batches_[it->second].instances.push_back(inst);
        }
    }
}

void ImpostorSystem::RenderOpaque(CommandBuffer& cmd, const RenderScenePassContext& ctx) {
    if (batches_.empty() || !device_) return;
    // 使用 Pass 传入的帧级别 view/projection，覆盖可能过时的缓存值
    glm::mat4 view = ctx.view ? *ctx.view : view_;
    glm::mat4 proj = ctx.projection ? *ctx.projection : proj_;
    // Camera-Relative: 需要将 view 平移 -camera_offset（与 particle 保持一致）
    glm::mat4 world_to_view = view * glm::translate(glm::mat4(1.0f), -ctx.camera_offset);
    renderer_.DrawImpostors(cmd, *device_, batches_,
                            world_to_view, proj, camera_pos_,
                            light_dir_, ambient_color_);
}

void ImpostorSystem::SetRenderContext(RhiDevice* device,
                                      const glm::mat4& view,
                                      const glm::mat4& proj,
                                      const glm::vec3& camera_pos,
                                      const glm::vec3& light_dir,
                                      const glm::vec3& ambient_color) {
    device_ = device;
    view_ = view;
    proj_ = proj;
    camera_pos_ = camera_pos;
    light_dir_ = light_dir;
    ambient_color_ = ambient_color;
}

void ImpostorSystem::Shutdown(RhiDevice& device) {
    renderer_.Shutdown(device);
}

void ImpostorSystem::ComputeFrameIndex(const glm::vec3& view_dir,
                                       ImpostorFrameMode mode,
                                       int frames_x, int frames_y,
                                       int& out_frame_x, int& out_frame_y) {
    if (frames_x <= 0 || frames_y <= 0) {
        out_frame_x = out_frame_y = 0;
        return;
    }

    // 视线方向到球面坐标
    // view_dir 从相机指向物体（已归一化）
    float azimuth = std::atan2(view_dir.x, view_dir.z);  // [-π, π]
    float elevation = std::asin(std::clamp(view_dir.y, -1.0f, 1.0f));  // [-π/2, π/2]

    switch (mode) {
    case ImpostorFrameMode::HemiOctahedron: {
        // 半球映射：水平角 [0, 2π] → frames_x，仰角 [0, π/2] → frames_y
        float norm_az = (azimuth + glm::pi<float>()) / (2.0f * glm::pi<float>());
        float norm_el = std::clamp(elevation / (glm::half_pi<float>()), 0.0f, 1.0f);

        out_frame_x = std::clamp(static_cast<int>(norm_az * frames_x), 0, frames_x - 1);
        out_frame_y = std::clamp(static_cast<int>(norm_el * frames_y), 0, frames_y - 1);
        break;
    }
    case ImpostorFrameMode::FullOctahedron: {
        // 全球映射：水平角 [0, 2π] → frames_x，仰角 [-π/2, π/2] → frames_y
        float norm_az = (azimuth + glm::pi<float>()) / (2.0f * glm::pi<float>());
        float norm_el = (elevation + glm::half_pi<float>()) / glm::pi<float>();

        out_frame_x = std::clamp(static_cast<int>(norm_az * frames_x), 0, frames_x - 1);
        out_frame_y = std::clamp(static_cast<int>(norm_el * frames_y), 0, frames_y - 1);
        break;
    }
    case ImpostorFrameMode::Billboard: {
        // 仅水平旋转
        float norm_az = (azimuth + glm::pi<float>()) / (2.0f * glm::pi<float>());
        out_frame_x = std::clamp(static_cast<int>(norm_az * frames_x), 0, frames_x - 1);
        out_frame_y = 0;
        break;
    }
    }
}

} // namespace render
} // namespace dse
