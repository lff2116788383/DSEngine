/**
 * @file meshlet_cull_pass.cpp
 * @brief Meshlet Cull Pass 实现
 */

#include "engine/render/meshlet/meshlet_cull_pass.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace render {

uint32_t MeshletCullPass::RegisterMesh(const MeshletMesh& mesh) {
    uint32_t id = next_mesh_id_++;
    MeshletRegistryEntry entry;
    entry.mesh_data = mesh;
    entry.global_vertex_offset = 0;
    entry.global_index_offset = 0;
    mesh_registry_[id] = std::move(entry);
    return id;
}

void MeshletCullPass::UnregisterMesh(uint32_t mesh_id) {
    mesh_registry_.erase(mesh_id);
}

void MeshletCullPass::AddInstance(uint32_t mesh_id, const glm::mat4& model) {
    auto it = mesh_registry_.find(mesh_id);
    if (it == mesh_registry_.end()) return;

    MeshletInstance inst;
    inst.mesh_id = mesh_id;
    inst.model = model;
    inst.base_meshlet = total_meshlet_count_;
    inst.meshlet_count = static_cast<uint32_t>(it->second.mesh_data.meshlets.size());
    total_meshlet_count_ += inst.meshlet_count;
    instances_.push_back(inst);
}

void MeshletCullPass::BeginFrame() {
    instances_.clear();
    draw_commands_.clear();
    meshlet_gpu_data_.clear();
    total_meshlet_count_ = 0;
}

uint32_t MeshletCullPass::PrepareGPUData(const glm::mat4& view, const glm::mat4& proj,
                                          const glm::vec3& camera_pos) {
    (void)view; (void)proj; (void)camera_pos;

    draw_commands_.clear();
    meshlet_gpu_data_.clear();
    draw_commands_.reserve(total_meshlet_count_);
    meshlet_gpu_data_.reserve(total_meshlet_count_);

    for (const auto& inst : instances_) {
        auto it = mesh_registry_.find(inst.mesh_id);
        if (it == mesh_registry_.end()) continue;

        const auto& mesh_data = it->second.mesh_data;
        for (uint32_t mi = 0; mi < inst.meshlet_count; ++mi) {
            const Meshlet& m = mesh_data.meshlets[mi];
            const auto& range = mesh_data.draw_ranges[mi];

            // Transform bounding sphere to world space
            glm::vec4 world_center = inst.model * glm::vec4(m.center, 1.0f);
            // Approximate uniform scale for radius
            float scale = glm::length(glm::vec3(inst.model[0]));
            float world_radius = m.radius * scale;

            // Transform cone axis to world space (rotation only)
            glm::vec3 world_cone_axis = glm::normalize(glm::mat3(inst.model) * m.cone_axis);

            MeshletGPUData gpu_data;
            gpu_data.sphere = glm::vec4(glm::vec3(world_center), world_radius);
            gpu_data.cone = glm::vec4(world_cone_axis, m.cone_cutoff);
            meshlet_gpu_data_.push_back(gpu_data);

            MeshletDrawCommand cmd;
            cmd.count = range.index_count;
            cmd.instance_count = 1; // visible by default, culling sets to 0
            cmd.first_index = range.index_offset + it->second.global_index_offset;
            cmd.base_vertex = static_cast<int32_t>(it->second.global_vertex_offset);
            cmd.base_instance = 0;
            draw_commands_.push_back(cmd);
        }
    }

    return static_cast<uint32_t>(draw_commands_.size());
}

void MeshletCullPass::CullCPU(const glm::mat4& view_proj, const glm::vec3& camera_pos,
                               const MeshletCullConfig& config) {
    glm::vec4 frustum_planes[6];
    ExtractFrustumPlanes(view_proj, frustum_planes);

    for (uint32_t i = 0; i < draw_commands_.size(); ++i) {
        const auto& gpu = meshlet_gpu_data_[i];
        glm::vec3 center(gpu.sphere.x, gpu.sphere.y, gpu.sphere.z);
        float radius = gpu.sphere.w;

        bool visible = true;

        // Frustum culling
        if (config.enable_frustum_cull) {
            if (!FrustumTestSphere(frustum_planes, center, radius)) {
                visible = false;
            }
        }

        // Normal cone culling
        if (visible && config.enable_cone_cull) {
            glm::vec3 cone_axis(gpu.cone.x, gpu.cone.y, gpu.cone.z);
            float cone_cutoff = gpu.cone.w;
            if (cone_cutoff > config.cone_cull_threshold) {
                if (ConeCullTest(cone_axis, cone_cutoff, camera_pos, center)) {
                    visible = false;
                }
            }
        }

        draw_commands_[i].instance_count = visible ? 1u : 0u;
    }
}

uint32_t MeshletCullPass::GetVisibleMeshletCount() const {
    uint32_t count = 0;
    for (const auto& cmd : draw_commands_) {
        if (cmd.instance_count > 0) ++count;
    }
    return count;
}

void MeshletCullPass::ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]) {
    // Left
    planes[0] = glm::vec4(vp[0][3] + vp[0][0], vp[1][3] + vp[1][0],
                           vp[2][3] + vp[2][0], vp[3][3] + vp[3][0]);
    // Right
    planes[1] = glm::vec4(vp[0][3] - vp[0][0], vp[1][3] - vp[1][0],
                           vp[2][3] - vp[2][0], vp[3][3] - vp[3][0]);
    // Bottom
    planes[2] = glm::vec4(vp[0][3] + vp[0][1], vp[1][3] + vp[1][1],
                           vp[2][3] + vp[2][1], vp[3][3] + vp[3][1]);
    // Top
    planes[3] = glm::vec4(vp[0][3] - vp[0][1], vp[1][3] - vp[1][1],
                           vp[2][3] - vp[2][1], vp[3][3] - vp[3][1]);
    // Near
    planes[4] = glm::vec4(vp[0][3] + vp[0][2], vp[1][3] + vp[1][2],
                           vp[2][3] + vp[2][2], vp[3][3] + vp[3][2]);
    // Far
    planes[5] = glm::vec4(vp[0][3] - vp[0][2], vp[1][3] - vp[1][2],
                           vp[2][3] - vp[2][2], vp[3][3] - vp[3][2]);

    // Normalize planes
    for (int i = 0; i < 6; ++i) {
        float len = glm::length(glm::vec3(planes[i]));
        if (len > 1e-7f) planes[i] /= len;
    }
}

bool MeshletCullPass::FrustumTestSphere(const glm::vec4 planes[6],
                                         const glm::vec3& center, float radius) {
    for (int i = 0; i < 6; ++i) {
        float dist = glm::dot(glm::vec3(planes[i]), center) + planes[i].w;
        if (dist < -radius) return false;
    }
    return true;
}

bool MeshletCullPass::ConeCullTest(const glm::vec3& cone_axis, float cone_cutoff,
                                    const glm::vec3& camera_pos, const glm::vec3& center) {
    // If camera is behind the normal cone (all triangles face away), cull
    glm::vec3 to_camera = glm::normalize(camera_pos - center);
    float dot = glm::dot(to_camera, cone_axis);
    // If dot <= -cone_cutoff, camera is beyond the cone boundary → all tris face away
    return dot <= -cone_cutoff;
}

} // namespace render
} // namespace dse
