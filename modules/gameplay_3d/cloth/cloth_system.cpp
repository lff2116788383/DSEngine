#include "modules/gameplay_3d/cloth/cloth_system.h"
#include "engine/ecs/components_3d.h"
#include "engine/ecs/components_3d_cloth.h"
#include "engine/ecs/transform.h"
#include "engine/assets/asset_manager.h"
#include "engine/base/debug.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <cmath>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>

namespace {

/// Hash for edge (unordered pair)
struct EdgeHash {
    std::size_t operator()(const std::pair<uint32_t, uint32_t>& e) const {
        uint32_t a = std::min(e.first, e.second);
        uint32_t b = std::max(e.first, e.second);
        return std::hash<uint64_t>()(static_cast<uint64_t>(a) << 32 | b);
    }
};

struct EdgeEqual {
    bool operator()(const std::pair<uint32_t, uint32_t>& a,
                    const std::pair<uint32_t, uint32_t>& b) const {
        uint32_t a0 = std::min(a.first, a.second), a1 = std::max(a.first, a.second);
        uint32_t b0 = std::min(b.first, b.second), b1 = std::max(b.first, b.second);
        return a0 == b0 && a1 == b1;
    }
};

/// Compute dihedral angle between two triangles sharing an edge
float DihedralAngle(const glm::vec3& p0, const glm::vec3& p1,
                    const glm::vec3& p2, const glm::vec3& p3) {
    // p1-p2 is the shared edge
    // p0 is opposite vertex of triangle 1
    // p3 is opposite vertex of triangle 2
    glm::vec3 n1 = glm::cross(p1 - p0, p2 - p0);
    glm::vec3 n2 = glm::cross(p1 - p3, p2 - p3);
    float len1 = glm::length(n1);
    float len2 = glm::length(n2);
    if (len1 < 1e-8f || len2 < 1e-8f) return 0.0f;
    n1 /= len1;
    n2 /= len2;
    float dot = glm::clamp(glm::dot(n1, n2), -1.0f, 1.0f);
    return std::acos(dot);
}

} // namespace

namespace dse {
namespace gameplay3d {

void ClothSystem::SetAssetManager(AssetManager* asset_manager) {
    asset_manager_ = asset_manager;
}

void ClothSystem::InitializeCloth(World& world, entt::entity entity, ClothComponent& cloth) {
    if (cloth.initialized) return;

    // Try to get particle positions from MeshRendererComponent's loaded vertex data
    if (world.registry().all_of<MeshRendererComponent>(entity)) {
        auto& mr = world.registry().get<MeshRendererComponent>(entity);
        if (!mr.temp_vertices.empty() && !mr.temp_indices.empty()) {
            bool is_dmesh = mr.mesh_path.find(".dmesh") != std::string::npos;
            size_t stride = is_dmesh ? static_cast<size_t>(mr.dmesh_vertex_stride) : 3;
            size_t vcount = mr.temp_vertices.size() / stride;

            cloth.particle_count = static_cast<uint32_t>(vcount);
            cloth.positions.resize(vcount);
            cloth.prev_positions.resize(vcount);
            cloth.velocities.resize(vcount, glm::vec3(0.0f));
            cloth.inv_masses.resize(vcount, 1.0f);
            cloth.rest_positions.resize(vcount);
            cloth.normals.resize(vcount, glm::vec3(0.0f, 1.0f, 0.0f));
            cloth.uvs.resize(vcount, glm::vec2(0.0f));

            // Get world transform
            glm::mat4 model(1.0f);
            if (world.registry().all_of<TransformComponent>(entity)) {
                auto& t = world.registry().get<TransformComponent>(entity);
                model = glm::translate(glm::mat4(1.0f), t.position)
                      * glm::mat4_cast(t.rotation)
                      * glm::scale(glm::mat4(1.0f), t.scale);
            }

            for (size_t i = 0; i < vcount; ++i) {
                glm::vec3 local_pos(
                    mr.temp_vertices[i * stride + 0],
                    mr.temp_vertices[i * stride + 1],
                    mr.temp_vertices[i * stride + 2]);
                cloth.rest_positions[i] = local_pos;

                glm::vec4 world_pos = model * glm::vec4(local_pos, 1.0f);
                cloth.positions[i] = glm::vec3(world_pos);
                cloth.prev_positions[i] = cloth.positions[i];

                if (is_dmesh && stride >= 8) {
                    cloth.uvs[i] = glm::vec2(
                        mr.temp_vertices[i * stride + 6],
                        mr.temp_vertices[i * stride + 7]);
                }
            }

            // Copy triangle indices
            cloth.triangle_indices.resize(mr.temp_indices.size());
            for (size_t i = 0; i < mr.temp_indices.size(); ++i) {
                cloth.triangle_indices[i] = static_cast<uint32_t>(mr.temp_indices[i]);
            }
        }
    }

    if (cloth.particle_count == 0) {
        DEBUG_LOG_WARN("[ClothSystem] Entity {} has no mesh data for cloth init",
                       static_cast<uint32_t>(entity));
        return;
    }

    // Set pinned particles to inv_mass = 0
    for (uint32_t pin : cloth.pinned_vertices) {
        if (pin < cloth.particle_count) {
            cloth.inv_masses[pin] = 0.0f;
        }
    }

    // Build constraints from topology
    BuildConstraints(cloth);

    cloth.initialized = true;
    cloth.mesh_dirty = true;

    DEBUG_LOG_INFO("[ClothSystem] Initialized cloth: {} particles, {} dist constraints, {} bend constraints",
                   cloth.particle_count, cloth.distance_constraints.size(), cloth.bend_constraints.size());
}

void ClothSystem::BuildConstraints(ClothComponent& cloth) {
    // Distance constraints: one per unique edge
    std::unordered_set<std::pair<uint32_t, uint32_t>, EdgeHash, EdgeEqual> edges;

    // Also build edge→triangles map for bending constraints
    // edge → vector of opposite vertex indices
    using Edge = std::pair<uint32_t, uint32_t>;
    std::unordered_map<Edge, std::vector<uint32_t>, EdgeHash, EdgeEqual> edge_opposite;

    for (size_t t = 0; t + 2 < cloth.triangle_indices.size(); t += 3) {
        uint32_t i0 = cloth.triangle_indices[t + 0];
        uint32_t i1 = cloth.triangle_indices[t + 1];
        uint32_t i2 = cloth.triangle_indices[t + 2];

        Edge e01(std::min(i0, i1), std::max(i0, i1));
        Edge e12(std::min(i1, i2), std::max(i1, i2));
        Edge e02(std::min(i0, i2), std::max(i0, i2));

        edges.insert(e01);
        edges.insert(e12);
        edges.insert(e02);

        // For bending: edge → opposite vertex
        edge_opposite[e01].push_back(i2);
        edge_opposite[e12].push_back(i0);
        edge_opposite[e02].push_back(i1);
    }

    // Create distance constraints
    cloth.distance_constraints.clear();
    cloth.distance_constraints.reserve(edges.size());
    for (const auto& edge : edges) {
        ClothDistanceConstraint c;
        c.i = edge.first;
        c.j = edge.second;
        c.rest_length = glm::distance(cloth.positions[c.i], cloth.positions[c.j]);
        if (c.rest_length < 1e-8f) c.rest_length = 1e-8f;
        cloth.distance_constraints.push_back(c);
    }

    // Create bending constraints (edges shared by exactly 2 triangles)
    cloth.bend_constraints.clear();
    for (const auto& [edge, opposites] : edge_opposite) {
        if (opposites.size() == 2) {
            ClothBendConstraint bc;
            bc.i0 = opposites[0]; // opposite vertex of tri 1
            bc.i1 = edge.first;   // shared edge vertex A
            bc.i2 = edge.second;  // shared edge vertex B
            bc.i3 = opposites[1]; // opposite vertex of tri 2
            bc.rest_angle = DihedralAngle(
                cloth.positions[bc.i0], cloth.positions[bc.i1],
                cloth.positions[bc.i2], cloth.positions[bc.i3]);
            cloth.bend_constraints.push_back(bc);
        }
    }
}

void ClothSystem::FixedUpdate(World& world, float dt) {
    if (dt <= 0.0f) return;

    auto view = world.registry().view<ClothComponent>();
    for (auto entity : view) {
        auto& cloth = view.get<ClothComponent>(entity);
        if (!cloth.enabled) continue;

        if (!cloth.initialized) {
            InitializeCloth(world, entity, cloth);
            if (!cloth.initialized) continue;
        }

        // Update pinned vertices to follow transform
        if (world.registry().all_of<TransformComponent>(entity)) {
            auto& t = world.registry().get<TransformComponent>(entity);
            glm::mat4 model = glm::translate(glm::mat4(1.0f), t.position)
                            * glm::mat4_cast(t.rotation)
                            * glm::scale(glm::mat4(1.0f), t.scale);
            for (uint32_t pin : cloth.pinned_vertices) {
                if (pin < cloth.particle_count) {
                    glm::vec4 wp = model * glm::vec4(cloth.rest_positions[pin], 1.0f);
                    cloth.positions[pin] = glm::vec3(wp);
                    cloth.prev_positions[pin] = cloth.positions[pin];
                }
            }
        }

        // 1. Predict positions (explicit Euler + Verlet-like)
        PredictPositions(cloth, dt);

        // 2. Constraint projection (multiple iterations)
        for (uint32_t iter = 0; iter < cloth.solver_iterations; ++iter) {
            ProjectDistanceConstraints(cloth);
            ProjectBendConstraints(cloth);
            ProjectCollisionConstraints(world, cloth);
        }

        // 3. Update velocities
        UpdateVelocities(cloth, dt);

        // 4. Recompute normals
        RecomputeNormals(cloth);

        cloth.mesh_dirty = true;

        // 5. Sync simulated positions back to MeshRendererComponent
        if (cloth.mesh_dirty && world.registry().all_of<MeshRendererComponent>(entity)) {
            auto& mr = world.registry().get<MeshRendererComponent>(entity);
            const bool is_dmesh = mr.mesh_path.find(".dmesh") != std::string::npos;
            const size_t stride = is_dmesh ? static_cast<size_t>(mr.dmesh_vertex_stride) : 3;
            const size_t expected = cloth.particle_count * stride;
            if (mr.temp_vertices.size() >= expected) {
                // Compute inverse model to transform world positions back to local space
                glm::mat4 inv_model(1.0f);
                if (world.registry().all_of<TransformComponent>(entity)) {
                    auto& t = world.registry().get<TransformComponent>(entity);
                    glm::mat4 model = glm::translate(glm::mat4(1.0f), t.position)
                                    * glm::mat4_cast(t.rotation)
                                    * glm::scale(glm::mat4(1.0f), t.scale);
                    inv_model = glm::inverse(model);
                }
                for (uint32_t i = 0; i < cloth.particle_count; ++i) {
                    glm::vec3 local = glm::vec3(inv_model * glm::vec4(cloth.positions[i], 1.0f));
                    mr.temp_vertices[i * stride + 0] = local.x;
                    mr.temp_vertices[i * stride + 1] = local.y;
                    mr.temp_vertices[i * stride + 2] = local.z;
                    // Update normals for proper lighting
                    if (i < static_cast<uint32_t>(cloth.normals.size())) {
                        glm::vec3 local_n = glm::normalize(glm::mat3(inv_model) * cloth.normals[i]);
                        if (is_dmesh && stride >= 6) {
                            // dmesh: normals at [3,4,5] in temp_vertices
                            mr.temp_vertices[i * stride + 3] = local_n.x;
                            mr.temp_vertices[i * stride + 4] = local_n.y;
                            mr.temp_vertices[i * stride + 5] = local_n.z;
                        } else if (mr.temp_normals.size() >= cloth.particle_count * 3) {
                            // Lua mesh: normals in separate temp_normals array
                            mr.temp_normals[i * 3 + 0] = local_n.x;
                            mr.temp_normals[i * 3 + 1] = local_n.y;
                            mr.temp_normals[i * 3 + 2] = local_n.z;
                        }
                    }
                }
            }
        }
    }
}

void ClothSystem::PredictPositions(ClothComponent& cloth, float dt) {
    for (uint32_t i = 0; i < cloth.particle_count; ++i) {
        if (cloth.inv_masses[i] <= 0.0f) continue; // pinned

        // Apply gravity
        cloth.velocities[i] += cloth.gravity * dt;

        // Apply wind (with optional turbulence)
        glm::vec3 wind = cloth.wind;
        if (cloth.wind_turbulence > 0.0f) {
            // Simple pseudo-random per particle based on position
            float noise = std::sin(cloth.positions[i].x * 12.9898f +
                                   cloth.positions[i].z * 78.233f +
                                   cloth.positions[i].y * 37.719f) * 43758.5453f;
            noise = noise - std::floor(noise); // fract
            wind += glm::vec3(noise - 0.5f) * cloth.wind_turbulence * 2.0f;
        }
        cloth.velocities[i] += wind * cloth.inv_masses[i] * dt;

        // Damping
        cloth.velocities[i] *= (1.0f - cloth.damping);

        // Store previous and predict
        cloth.prev_positions[i] = cloth.positions[i];
        cloth.positions[i] += cloth.velocities[i] * dt;
    }
}

void ClothSystem::ProjectDistanceConstraints(ClothComponent& cloth) {
    // XPBD distance constraint projection
    // compliance alpha = (1 - stiffness) / (dt^2 * stiffness) simplified to direct stiffness
    float k = glm::clamp(cloth.stiffness, 0.0f, 1.0f);

    for (const auto& c : cloth.distance_constraints) {
        glm::vec3& p1 = cloth.positions[c.i];
        glm::vec3& p2 = cloth.positions[c.j];
        float w1 = cloth.inv_masses[c.i];
        float w2 = cloth.inv_masses[c.j];
        float w_sum = w1 + w2;
        if (w_sum < 1e-10f) continue;

        glm::vec3 delta = p2 - p1;
        float dist = glm::length(delta);
        if (dist < 1e-10f) continue;

        float error = dist - c.rest_length;
        glm::vec3 correction = (error / dist) * (delta / w_sum) * k;

        p1 += correction * w1;
        p2 -= correction * w2;
    }
}

void ClothSystem::ProjectBendConstraints(ClothComponent& cloth) {
    if (cloth.bend_stiffness <= 0.0f) return;
    float k = glm::clamp(cloth.bend_stiffness, 0.0f, 1.0f);

    for (const auto& bc : cloth.bend_constraints) {
        glm::vec3& p0 = cloth.positions[bc.i0];
        glm::vec3& p1 = cloth.positions[bc.i1];
        glm::vec3& p2 = cloth.positions[bc.i2];
        glm::vec3& p3 = cloth.positions[bc.i3];

        float current_angle = DihedralAngle(p0, p1, p2, p3);
        float angle_error = current_angle - bc.rest_angle;

        if (std::abs(angle_error) < 1e-6f) continue;

        // Simplified bending correction: push opposite vertices along their normals
        glm::vec3 n1 = glm::cross(p1 - p0, p2 - p0);
        float n1_len = glm::length(n1);
        if (n1_len < 1e-8f) continue;
        n1 /= n1_len;

        glm::vec3 n2 = glm::cross(p1 - p3, p2 - p3);
        float n2_len = glm::length(n2);
        if (n2_len < 1e-8f) continue;
        n2 /= n2_len;

        float correction = angle_error * k * 0.1f; // Scaled down for stability

        float w0 = cloth.inv_masses[bc.i0];
        float w3 = cloth.inv_masses[bc.i3];
        float w_sum = w0 + w3;
        if (w_sum < 1e-10f) continue;

        p0 += n1 * correction * (w0 / w_sum);
        p3 -= n2 * correction * (w3 / w_sum);
    }
}

void ClothSystem::ProjectCollisionConstraints(World& world, ClothComponent& cloth) {
    // Sphere colliders
    for (const auto& sc : cloth.sphere_colliders) {
        auto sc_ent = static_cast<entt::entity>(sc.entity_id);
        if (!world.registry().valid(sc_ent)) continue;
        if (!world.registry().all_of<TransformComponent>(sc_ent)) continue;

        const auto& t = world.registry().get<TransformComponent>(sc_ent);
        glm::vec3 center = t.position;
        float radius = sc.radius * glm::max(t.scale.x, glm::max(t.scale.y, t.scale.z));
        float total_radius = radius + cloth.collision_radius;

        for (uint32_t i = 0; i < cloth.particle_count; ++i) {
            if (cloth.inv_masses[i] <= 0.0f) continue;

            glm::vec3 diff = cloth.positions[i] - center;
            float dist = glm::length(diff);
            if (dist < total_radius && dist > 1e-8f) {
                glm::vec3 normal = diff / dist;
                cloth.positions[i] = center + normal * total_radius;

                // Apply friction: reduce tangential velocity
                glm::vec3& vel = cloth.velocities[i];
                float vn = glm::dot(vel, normal);
                glm::vec3 vt = vel - vn * normal;
                vel = vn * normal + vt * (1.0f - cloth.friction);
            }
        }
    }

    // Capsule colliders
    for (const auto& cc : cloth.capsule_colliders) {
        auto cc_ent = static_cast<entt::entity>(cc.entity_id);
        if (!world.registry().valid(cc_ent)) continue;
        if (!world.registry().all_of<TransformComponent>(cc_ent)) continue;

        const auto& t = world.registry().get<TransformComponent>(cc_ent);
        glm::vec3 center = t.position;
        glm::vec3 up = t.rotation * glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 cap_a = center - up * cc.half_height;
        glm::vec3 cap_b = center + up * cc.half_height;
        float radius = cc.radius + cloth.collision_radius;

        for (uint32_t i = 0; i < cloth.particle_count; ++i) {
            if (cloth.inv_masses[i] <= 0.0f) continue;

            // Closest point on capsule segment to particle
            glm::vec3 ab = cap_b - cap_a;
            float ab_len2 = glm::dot(ab, ab);
            if (ab_len2 < 1e-10f) continue;

            float proj = glm::clamp(glm::dot(cloth.positions[i] - cap_a, ab) / ab_len2, 0.0f, 1.0f);
            glm::vec3 closest = cap_a + proj * ab;

            glm::vec3 diff = cloth.positions[i] - closest;
            float dist = glm::length(diff);
            if (dist < radius && dist > 1e-8f) {
                glm::vec3 normal = diff / dist;
                cloth.positions[i] = closest + normal * radius;

                glm::vec3& vel = cloth.velocities[i];
                float vn = glm::dot(vel, normal);
                glm::vec3 vt = vel - vn * normal;
                vel = vn * normal + vt * (1.0f - cloth.friction);
            }
        }
    }

    // Ground plane collision (Y=0 as simple fallback)
    for (uint32_t i = 0; i < cloth.particle_count; ++i) {
        if (cloth.inv_masses[i] <= 0.0f) continue;
        float floor_y = cloth.collision_radius;
        if (cloth.positions[i].y < floor_y) {
            cloth.positions[i].y = floor_y;
            if (cloth.velocities[i].y < 0.0f) {
                cloth.velocities[i].y = 0.0f;
            }
        }
    }
}

void ClothSystem::UpdateVelocities(ClothComponent& cloth, float dt) {
    float inv_dt = 1.0f / dt;
    for (uint32_t i = 0; i < cloth.particle_count; ++i) {
        if (cloth.inv_masses[i] <= 0.0f) {
            cloth.velocities[i] = glm::vec3(0.0f);
            continue;
        }
        cloth.velocities[i] = (cloth.positions[i] - cloth.prev_positions[i]) * inv_dt;
    }
}

void ClothSystem::RecomputeNormals(ClothComponent& cloth) {
    // Reset normals
    for (uint32_t i = 0; i < cloth.particle_count; ++i) {
        cloth.normals[i] = glm::vec3(0.0f);
    }

    // Accumulate face normals
    for (size_t t = 0; t + 2 < cloth.triangle_indices.size(); t += 3) {
        uint32_t i0 = cloth.triangle_indices[t + 0];
        uint32_t i1 = cloth.triangle_indices[t + 1];
        uint32_t i2 = cloth.triangle_indices[t + 2];

        if (i0 >= cloth.particle_count || i1 >= cloth.particle_count || i2 >= cloth.particle_count)
            continue;

        glm::vec3 e1 = cloth.positions[i1] - cloth.positions[i0];
        glm::vec3 e2 = cloth.positions[i2] - cloth.positions[i0];
        glm::vec3 face_normal = glm::cross(e1, e2);

        cloth.normals[i0] += face_normal;
        cloth.normals[i1] += face_normal;
        cloth.normals[i2] += face_normal;
    }

    // Normalize
    for (uint32_t i = 0; i < cloth.particle_count; ++i) {
        float len = glm::length(cloth.normals[i]);
        if (len > 1e-8f) {
            cloth.normals[i] /= len;
        } else {
            cloth.normals[i] = glm::vec3(0.0f, 1.0f, 0.0f);
        }
    }
}

} // namespace gameplay3d
} // namespace dse
