/**
 * @file meshlet_builder.cpp
 * @brief Meshlet 离线构建器实现
 *
 * 贪心空间局部性聚类：从未分配三角形中选择种子，按邻接关系扩展直到满 cluster。
 * 输出每个 meshlet 的局部顶点表、三角形表、包围球、法线锥。
 */

#include "engine/render/meshlet/meshlet_builder.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace dse {
namespace render {

MeshletMesh MeshletBuilder::Build(const std::vector<glm::vec3>& positions,
                                   const std::vector<uint32_t>& indices,
                                   const MeshletBuildConfig& config) {
    MeshletMesh result;
    result.positions = positions;

    const uint32_t tri_count = static_cast<uint32_t>(indices.size() / 3);
    if (tri_count == 0) return result;

    const uint32_t max_verts = std::min(config.max_vertices, kMaxMeshletVertices);
    const uint32_t max_tris = std::min(config.max_triangles, kMaxMeshletTriangles);

    // Build adjacency: for each vertex, list of triangles using it
    std::vector<std::vector<uint32_t>> vert_to_tris(positions.size());
    for (uint32_t t = 0; t < tri_count; ++t) {
        vert_to_tris[indices[t * 3 + 0]].push_back(t);
        vert_to_tris[indices[t * 3 + 1]].push_back(t);
        vert_to_tris[indices[t * 3 + 2]].push_back(t);
    }

    std::vector<bool> tri_used(tri_count, false);
    uint32_t remaining = tri_count;

    while (remaining > 0) {
        // Find seed: first unused triangle
        uint32_t seed = 0;
        for (uint32_t t = 0; t < tri_count; ++t) {
            if (!tri_used[t]) { seed = t; break; }
        }

        // Current meshlet state
        std::unordered_set<uint32_t> meshlet_vert_set;
        std::vector<uint32_t> meshlet_vert_list;
        std::vector<uint32_t> meshlet_tri_list;

        // BFS queue of candidate triangles
        std::queue<uint32_t> candidates;
        candidates.push(seed);

        while (!candidates.empty() && meshlet_tri_list.size() < max_tris) {
            uint32_t tri = candidates.front();
            candidates.pop();

            if (tri_used[tri]) continue;

            // Check vertex budget
            uint32_t new_verts = 0;
            for (int k = 0; k < 3; ++k) {
                uint32_t vi = indices[tri * 3 + k];
                if (meshlet_vert_set.find(vi) == meshlet_vert_set.end())
                    ++new_verts;
            }
            if (meshlet_vert_set.size() + new_verts > max_verts) {
                // Skip this triangle, but don't mark used
                continue;
            }

            // Add triangle
            tri_used[tri] = true;
            --remaining;
            meshlet_tri_list.push_back(tri);

            for (int k = 0; k < 3; ++k) {
                uint32_t vi = indices[tri * 3 + k];
                if (meshlet_vert_set.insert(vi).second) {
                    meshlet_vert_list.push_back(vi);
                }
            }

            // Enqueue adjacent triangles (share at least one vertex)
            for (int k = 0; k < 3; ++k) {
                uint32_t vi = indices[tri * 3 + k];
                for (uint32_t adj_tri : vert_to_tris[vi]) {
                    if (!tri_used[adj_tri]) {
                        candidates.push(adj_tri);
                    }
                }
            }
        }

        // Build meshlet descriptor
        Meshlet meshlet{};
        meshlet.vertex_offset = static_cast<uint32_t>(result.meshlet_vertices.size());
        meshlet.vertex_count = static_cast<uint32_t>(meshlet_vert_list.size());
        meshlet.triangle_offset = static_cast<uint32_t>(result.meshlet_triangles.size());
        meshlet.triangle_count = static_cast<uint32_t>(meshlet_tri_list.size());

        // Build local vertex index mapping
        std::unordered_map<uint32_t, uint8_t> global_to_local;
        for (uint32_t i = 0; i < meshlet_vert_list.size(); ++i) {
            result.meshlet_vertices.push_back(meshlet_vert_list[i]);
            global_to_local[meshlet_vert_list[i]] = static_cast<uint8_t>(i);
        }

        // Build triangle local indices and global draw indices
        MeshletMesh::DrawRange range;
        range.index_offset = static_cast<uint32_t>(result.global_indices.size());
        range.index_count = meshlet.triangle_count * 3;

        for (uint32_t tri : meshlet_tri_list) {
            for (int k = 0; k < 3; ++k) {
                uint32_t gi = indices[tri * 3 + k];
                result.meshlet_triangles.push_back(global_to_local[gi]);
                result.global_indices.push_back(gi);
            }
        }

        // Compute bounding sphere
        ComputeMeshletBounds(meshlet, positions, result.meshlet_vertices,
                             meshlet.vertex_offset, meshlet.vertex_count);

        // Compute normal cone
        ComputeNormalCone(meshlet, positions, result.meshlet_vertices,
                          result.meshlet_triangles,
                          meshlet.vertex_offset, meshlet.triangle_offset, meshlet.triangle_count);

        result.meshlets.push_back(meshlet);
        result.draw_ranges.push_back(range);
    }

    return result;
}

MeshletMesh MeshletBuilder::BuildFromFullMesh(const float* vertex_data, uint32_t vertex_count,
                                               uint32_t vertex_stride_floats,
                                               const uint32_t* indices_ptr, uint32_t index_count,
                                               const MeshletBuildConfig& config) {
    std::vector<glm::vec3> positions(vertex_count);
    for (uint32_t i = 0; i < vertex_count; ++i) {
        const float* v = vertex_data + i * vertex_stride_floats;
        positions[i] = glm::vec3(v[0], v[1], v[2]);
    }
    std::vector<uint32_t> indices(indices_ptr, indices_ptr + index_count);
    return Build(positions, indices, config);
}

void MeshletBuilder::ComputeMeshletBounds(Meshlet& meshlet, const std::vector<glm::vec3>& positions,
                                           const std::vector<uint32_t>& meshlet_vertices,
                                           uint32_t vertex_offset, uint32_t vertex_count) {
    if (vertex_count == 0) {
        meshlet.center = glm::vec3(0.0f);
        meshlet.radius = 0.0f;
        return;
    }

    // Compute centroid
    glm::vec3 center(0.0f);
    for (uint32_t i = 0; i < vertex_count; ++i) {
        center += positions[meshlet_vertices[vertex_offset + i]];
    }
    center /= static_cast<float>(vertex_count);

    // Compute radius (max distance from center)
    float max_dist_sq = 0.0f;
    for (uint32_t i = 0; i < vertex_count; ++i) {
        glm::vec3 p = positions[meshlet_vertices[vertex_offset + i]];
        float d = glm::dot(p - center, p - center);
        max_dist_sq = std::max(max_dist_sq, d);
    }

    meshlet.center = center;
    meshlet.radius = std::sqrt(max_dist_sq);
}

void MeshletBuilder::ComputeNormalCone(Meshlet& meshlet, const std::vector<glm::vec3>& positions,
                                        const std::vector<uint32_t>& meshlet_vertices,
                                        const std::vector<uint8_t>& meshlet_triangles,
                                        uint32_t vertex_offset, uint32_t tri_offset, uint32_t tri_count) {
    if (tri_count == 0) {
        meshlet.cone_axis = glm::vec3(0.0f, 0.0f, 1.0f);
        meshlet.cone_cutoff = -1.0f; // disabled
        return;
    }

    // Compute average normal
    glm::vec3 avg_normal(0.0f);
    for (uint32_t t = 0; t < tri_count; ++t) {
        uint8_t li0 = meshlet_triangles[tri_offset + t * 3 + 0];
        uint8_t li1 = meshlet_triangles[tri_offset + t * 3 + 1];
        uint8_t li2 = meshlet_triangles[tri_offset + t * 3 + 2];

        glm::vec3 v0 = positions[meshlet_vertices[vertex_offset + li0]];
        glm::vec3 v1 = positions[meshlet_vertices[vertex_offset + li1]];
        glm::vec3 v2 = positions[meshlet_vertices[vertex_offset + li2]];

        glm::vec3 n = glm::cross(v1 - v0, v2 - v0);
        float len = glm::length(n);
        if (len > 1e-7f) {
            avg_normal += n / len;
        }
    }

    float avg_len = glm::length(avg_normal);
    if (avg_len < 1e-7f) {
        meshlet.cone_axis = glm::vec3(0.0f, 0.0f, 1.0f);
        meshlet.cone_cutoff = -1.0f;
        return;
    }

    glm::vec3 axis = avg_normal / avg_len;

    // Find worst-case angle between axis and each triangle normal
    float min_dot = 1.0f;
    for (uint32_t t = 0; t < tri_count; ++t) {
        uint8_t li0 = meshlet_triangles[tri_offset + t * 3 + 0];
        uint8_t li1 = meshlet_triangles[tri_offset + t * 3 + 1];
        uint8_t li2 = meshlet_triangles[tri_offset + t * 3 + 2];

        glm::vec3 v0 = positions[meshlet_vertices[vertex_offset + li0]];
        glm::vec3 v1 = positions[meshlet_vertices[vertex_offset + li1]];
        glm::vec3 v2 = positions[meshlet_vertices[vertex_offset + li2]];

        glm::vec3 n = glm::normalize(glm::cross(v1 - v0, v2 - v0));
        min_dot = std::min(min_dot, glm::dot(n, axis));
    }

    meshlet.cone_axis = axis;
    meshlet.cone_cutoff = min_dot;
}

bool MeshletBuilder::Serialize(const MeshletMesh& mesh, const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    DmeshletHeader header{};
    header.magic = kDmeshletMagic;
    header.version = kDmeshletVersion;
    header.meshlet_count = static_cast<uint32_t>(mesh.meshlets.size());
    header.vertex_count = static_cast<uint32_t>(mesh.positions.size());
    header.meshlet_vertex_count = static_cast<uint32_t>(mesh.meshlet_vertices.size());
    header.meshlet_triangle_count = static_cast<uint32_t>(mesh.meshlet_triangles.size());
    header.global_index_count = static_cast<uint32_t>(mesh.global_indices.size());
    header.reserved = 0;

    file.write(reinterpret_cast<const char*>(&header), sizeof(header));

    // Meshlet descriptors
    file.write(reinterpret_cast<const char*>(mesh.meshlets.data()),
               mesh.meshlets.size() * sizeof(Meshlet));

    // Positions
    file.write(reinterpret_cast<const char*>(mesh.positions.data()),
               mesh.positions.size() * sizeof(glm::vec3));

    // Meshlet vertices
    file.write(reinterpret_cast<const char*>(mesh.meshlet_vertices.data()),
               mesh.meshlet_vertices.size() * sizeof(uint32_t));

    // Meshlet triangles
    file.write(reinterpret_cast<const char*>(mesh.meshlet_triangles.data()),
               mesh.meshlet_triangles.size());

    // Global indices
    file.write(reinterpret_cast<const char*>(mesh.global_indices.data()),
               mesh.global_indices.size() * sizeof(uint32_t));

    // Draw ranges
    file.write(reinterpret_cast<const char*>(mesh.draw_ranges.data()),
               mesh.draw_ranges.size() * sizeof(MeshletMesh::DrawRange));

    return file.good();
}

bool MeshletBuilder::Deserialize(const std::string& path, MeshletMesh& out_mesh) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) return false;

    DmeshletHeader header{};
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (header.magic != kDmeshletMagic || header.version != kDmeshletVersion)
        return false;

    out_mesh.meshlets.resize(header.meshlet_count);
    file.read(reinterpret_cast<char*>(out_mesh.meshlets.data()),
              header.meshlet_count * sizeof(Meshlet));

    out_mesh.positions.resize(header.vertex_count);
    file.read(reinterpret_cast<char*>(out_mesh.positions.data()),
              header.vertex_count * sizeof(glm::vec3));

    out_mesh.meshlet_vertices.resize(header.meshlet_vertex_count);
    file.read(reinterpret_cast<char*>(out_mesh.meshlet_vertices.data()),
              header.meshlet_vertex_count * sizeof(uint32_t));

    out_mesh.meshlet_triangles.resize(header.meshlet_triangle_count);
    file.read(reinterpret_cast<char*>(out_mesh.meshlet_triangles.data()),
              header.meshlet_triangle_count);

    out_mesh.global_indices.resize(header.global_index_count);
    file.read(reinterpret_cast<char*>(out_mesh.global_indices.data()),
              header.global_index_count * sizeof(uint32_t));

    out_mesh.draw_ranges.resize(header.meshlet_count);
    file.read(reinterpret_cast<char*>(out_mesh.draw_ranges.data()),
              header.meshlet_count * sizeof(MeshletMesh::DrawRange));

    return file.good();
}

} // namespace render
} // namespace dse
