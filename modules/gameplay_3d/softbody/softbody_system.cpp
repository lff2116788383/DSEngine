#include "modules/gameplay_3d/softbody/softbody_system.h"
#include "engine/ecs/components_3d_physics.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/transform.h"
#include "engine/base/debug.h"
#include <cmath>
#include <algorithm>
#include <unordered_set>

namespace dse {
namespace gameplay3d {

void SoftBodySystem::SetAssetManager(AssetManager* asset_manager) { asset_manager_ = asset_manager; }

void SoftBodySystem::FixedUpdate(World& world, float dt) {
    auto view = world.registry().view<SoftBodyComponent>();
    for (auto entity : view) {
        auto& sb = view.get<SoftBodyComponent>(entity);
        if (!sb.enabled) continue;

        if (!sb.initialized) {
            InitializeFromMesh(world, entity, sb);
        }
        if (sb.initialized) {
            Simulate(sb, dt);
            ProjectCollisions(world, sb);
            WriteBackMesh(world, entity, sb);
        }
    }
}

void SoftBodySystem::InitializeFromMesh(World& world, entt::entity entity, SoftBodyComponent& sb) {
    auto* mr = world.registry().try_get<MeshRendererComponent>(entity);
    if (!mr || mr->temp_vertices.empty() || mr->temp_indices.empty()) return;

    bool is_dmesh = mr->mesh_path.find(".dmesh") != std::string::npos;
    size_t stride = is_dmesh ? static_cast<size_t>(mr->dmesh_vertex_stride) : 3;
    size_t vcount = mr->temp_vertices.size() / stride;

    // 提取顶点位置
    sb.positions.resize(vcount);
    sb.prev_positions.resize(vcount);
    sb.velocities.resize(vcount, glm::vec3(0.0f));
    sb.inv_masses.resize(vcount, 1.0f);

    auto* transform = world.registry().try_get<TransformComponent>(entity);
    for (size_t i = 0; i < vcount; ++i) {
        glm::vec3 pos(mr->temp_vertices[i * stride + 0],
                      mr->temp_vertices[i * stride + 1],
                      mr->temp_vertices[i * stride + 2]);
        // 变换到世界空间
        if (transform) {
            glm::mat4 world_mat = glm::translate(glm::mat4(1.0f), transform->position)
                                * glm::mat4_cast(transform->rotation)
                                * glm::scale(glm::mat4(1.0f), transform->scale);
            pos = glm::vec3(world_mat * glm::vec4(pos, 1.0f));
        }
        sb.positions[i] = pos;
        sb.prev_positions[i] = pos;
    }

    // 构建边约束（从三角形索引提取唯一边）
    struct EdgeHash {
        size_t operator()(const std::pair<uint32_t, uint32_t>& e) const {
            return std::hash<uint64_t>()(static_cast<uint64_t>(e.first) << 32 | e.second);
        }
    };
    std::unordered_set<std::pair<uint32_t, uint32_t>, EdgeHash> edges;

    for (size_t i = 0; i + 2 < mr->temp_indices.size(); i += 3) {
        uint32_t a = mr->temp_indices[i];
        uint32_t b = mr->temp_indices[i + 1];
        uint32_t c = mr->temp_indices[i + 2];
        auto add_edge = [&](uint32_t u, uint32_t v) {
            auto key = std::make_pair(std::min(u, v), std::max(u, v));
            edges.insert(key);
        };
        add_edge(a, b);
        add_edge(b, c);
        add_edge(a, c);
    }

    sb.constraints.clear();
    sb.constraints.reserve(edges.size());
    for (const auto& [i0, i1] : edges) {
        SoftBodyDistConstraint c;
        c.i0 = i0;
        c.i1 = i1;
        c.rest_length = glm::length(sb.positions[i0] - sb.positions[i1]);
        sb.constraints.push_back(c);
    }

    // 计算初始体积
    sb.rest_volume = ComputeVolume(sb, mr->temp_indices);

    sb.initialized = true;
    sb.mesh_dirty = false;

    DEBUG_LOG_INFO("[SoftBody] Initialized: {} particles, {} constraints", vcount, sb.constraints.size());
}

void SoftBodySystem::Simulate(SoftBodyComponent& sb, float dt) {
    if (dt <= 0.0f) return;
    size_t n = sb.positions.size();

    // 1. 施加外力 + 预测位置
    glm::vec3 gravity(0.0f, -9.81f * sb.gravity_scale, 0.0f);
    for (size_t i = 0; i < n; ++i) {
        if (sb.inv_masses[i] <= 0.0f) continue; // 固定点
        sb.velocities[i] += (sb.use_gravity ? gravity : glm::vec3(0.0f)) * dt;
        sb.prev_positions[i] = sb.positions[i];
        sb.positions[i] += sb.velocities[i] * dt;
    }

    // 2. 约束投影
    for (int iter = 0; iter < sb.solver_iterations; ++iter) {
        ProjectDistanceConstraints(sb);
        ProjectVolumeConstraint(sb);
    }

    // 3. 更新速度 + 阻尼
    float inv_dt = 1.0f / dt;
    for (size_t i = 0; i < n; ++i) {
        if (sb.inv_masses[i] <= 0.0f) continue;
        sb.velocities[i] = (sb.positions[i] - sb.prev_positions[i]) * inv_dt;
        sb.velocities[i] *= sb.damping;
    }

    sb.mesh_dirty = true;
}

void SoftBodySystem::ProjectDistanceConstraints(SoftBodyComponent& sb) {
    float k = 1.0f - std::pow(1.0f - sb.stiffness, 1.0f / static_cast<float>(sb.solver_iterations));

    for (const auto& c : sb.constraints) {
        float w0 = sb.inv_masses[c.i0];
        float w1 = sb.inv_masses[c.i1];
        float w_sum = w0 + w1;
        if (w_sum <= 0.0f) continue;

        glm::vec3 diff = sb.positions[c.i1] - sb.positions[c.i0];
        float dist = glm::length(diff);
        if (dist < 1e-8f) continue;

        glm::vec3 dir = diff / dist;
        float delta = dist - c.rest_length;
        glm::vec3 correction = dir * delta * k;

        sb.positions[c.i0] += correction * (w0 / w_sum);
        sb.positions[c.i1] -= correction * (w1 / w_sum);
    }
}

void SoftBodySystem::ProjectVolumeConstraint(SoftBodyComponent& sb) {
    if (sb.rest_volume <= 0.0f || sb.volume_stiffness <= 0.0f) return;

    // 简化的体积保持：基于质心到粒子距离的均匀缩放
    glm::vec3 centroid(0.0f);
    float total_weight = 0.0f;
    for (size_t i = 0; i < sb.positions.size(); ++i) {
        float w = (sb.inv_masses[i] > 0.0f) ? 1.0f : 0.0f;
        centroid += sb.positions[i] * w;
        total_weight += w;
    }
    if (total_weight <= 0.0f) return;
    centroid /= total_weight;

    // 估算当前"体积"：用平均半径的立方近似
    float avg_r = 0.0f;
    for (size_t i = 0; i < sb.positions.size(); ++i) {
        if (sb.inv_masses[i] <= 0.0f) continue;
        avg_r += glm::length(sb.positions[i] - centroid);
    }
    avg_r /= total_weight;

    float rest_avg_r = std::cbrt(sb.rest_volume * 0.75f / 3.14159f);
    if (avg_r < 1e-6f || rest_avg_r < 1e-6f) return;

    float scale = 1.0f + (rest_avg_r / avg_r - 1.0f) * sb.volume_stiffness / static_cast<float>(sb.solver_iterations);

    for (size_t i = 0; i < sb.positions.size(); ++i) {
        if (sb.inv_masses[i] <= 0.0f) continue;
        glm::vec3 dir = sb.positions[i] - centroid;
        sb.positions[i] = centroid + dir * scale;
    }
}

void SoftBodySystem::ProjectCollisions(World& world, SoftBodyComponent& sb) {
    // 简单地面碰撞
    for (size_t i = 0; i < sb.positions.size(); ++i) {
        if (sb.inv_masses[i] <= 0.0f) continue;

        // 地形高度查询
        float ground_y = 0.0f;
        auto hm_view = world.registry().view<TerrainHeightmapComponent>();
        for (auto te : hm_view) {
            const auto& hm = hm_view.get<TerrainHeightmapComponent>(te);
            float h = hm.GetHeight(sb.positions[i].x, sb.positions[i].z);
            if (h > ground_y) ground_y = h;
        }

        if (sb.positions[i].y < ground_y) {
            sb.positions[i].y = ground_y;
        }
    }
}

void SoftBodySystem::WriteBackMesh(World& world, entt::entity entity, SoftBodyComponent& sb) {
    if (!sb.mesh_dirty) return;

    auto* mr = world.registry().try_get<MeshRendererComponent>(entity);
    auto* transform = world.registry().try_get<TransformComponent>(entity);
    if (!mr || mr->temp_vertices.empty()) return;

    bool is_dmesh = mr->mesh_path.find(".dmesh") != std::string::npos;
    size_t stride = is_dmesh ? static_cast<size_t>(mr->dmesh_vertex_stride) : 3;
    size_t vcount = mr->temp_vertices.size() / stride;
    if (vcount != sb.positions.size()) return;

    // 逆世界变换
    glm::mat4 inv_world = glm::mat4(1.0f);
    if (transform) {
        glm::mat4 world_mat = glm::translate(glm::mat4(1.0f), transform->position)
                            * glm::mat4_cast(transform->rotation)
                            * glm::scale(glm::mat4(1.0f), transform->scale);
        inv_world = glm::inverse(world_mat);
    }

    for (size_t i = 0; i < vcount; ++i) {
        glm::vec3 local_pos = glm::vec3(inv_world * glm::vec4(sb.positions[i], 1.0f));
        mr->temp_vertices[i * stride + 0] = local_pos.x;
        mr->temp_vertices[i * stride + 1] = local_pos.y;
        mr->temp_vertices[i * stride + 2] = local_pos.z;
    }

    sb.mesh_dirty = false;
}

float SoftBodySystem::ComputeVolume(const SoftBodyComponent& sb, const std::vector<uint32_t>& indices) const {
    float volume = 0.0f;
    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
        const glm::vec3& a = sb.positions[indices[i]];
        const glm::vec3& b = sb.positions[indices[i + 1]];
        const glm::vec3& c = sb.positions[indices[i + 2]];
        volume += glm::dot(a, glm::cross(b, c));
    }
    return std::abs(volume) / 6.0f;
}

} // namespace gameplay3d
} // namespace dse
