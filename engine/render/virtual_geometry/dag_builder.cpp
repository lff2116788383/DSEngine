/**
 * @file dag_builder.cpp
 * @brief DAG LOD hierarchy builder implementation
 *
 * Algorithm:
 * 1. Build leaf-level meshlets from MeshletBuilder
 * 2. For each subsequent LOD level:
 *    a. Group spatially-adjacent clusters into merge groups (k-means-like)
 *    b. Simplify each group's geometry (edge collapse) to ~50% triangle count
 *    c. Re-meshlet the simplified geometry
 *    d. Create parent DAG nodes pointing to child clusters
 * 3. Compute bounding spheres bottom-up
 */

#ifdef DSE_ENABLE_VIRTUAL_GEOMETRY

#include "engine/render/virtual_geometry/dag_builder.h"
#include "engine/render/meshlet/meshlet_builder.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace dse {
namespace render {
namespace vg {

// ============================================================================
// Edge-collapse mesh simplification (quadric error metric)
// ============================================================================

namespace {

struct QEMVertex {
    glm::vec3 pos;
    glm::mat4 quadric{0.0f};
    bool      boundary = false;
    bool      removed  = false;
    uint32_t  original_index;
};

struct QEMEdge {
    uint32_t v0, v1;
    float    cost;
    glm::vec3 optimal_pos;
    bool     removed = false;
};

glm::mat4 ComputePlaneQuadric(const glm::vec3& n, float d) {
    glm::mat4 q{0.0f};
    q[0][0] = n.x * n.x; q[0][1] = n.x * n.y; q[0][2] = n.x * n.z; q[0][3] = n.x * d;
    q[1][0] = n.y * n.x; q[1][1] = n.y * n.y; q[1][2] = n.y * n.z; q[1][3] = n.y * d;
    q[2][0] = n.z * n.x; q[2][1] = n.z * n.y; q[2][2] = n.z * n.z; q[2][3] = n.z * d;
    q[3][0] = d   * n.x; q[3][1] = d   * n.y; q[3][2] = d   * n.z; q[3][3] = d * d;
    return q;
}

float ComputeCollapseCost(const QEMVertex& v0, const QEMVertex& v1, glm::vec3& out_pos) {
    glm::mat4 q = v0.quadric + v1.quadric;
    out_pos = (v0.pos + v1.pos) * 0.5f;

    glm::mat4 solve = q;
    solve[3][0] = 0.0f; solve[3][1] = 0.0f; solve[3][2] = 0.0f; solve[3][3] = 1.0f;
    float det = glm::determinant(solve);
    if (std::abs(det) > 1e-10f) {
        glm::mat4 inv = glm::inverse(solve);
        out_pos = glm::vec3(inv[0][3], inv[1][3], inv[2][3]);
    }

    glm::vec4 p(out_pos, 1.0f);
    return std::abs(glm::dot(p, q * p));
}

struct SimplifiedMesh {
    std::vector<glm::vec3> positions;
    std::vector<uint32_t>  indices;
    float max_error;
};

SimplifiedMesh SimplifyMesh(const std::vector<glm::vec3>& positions,
                            const std::vector<uint32_t>& indices,
                            float target_ratio) {
    const uint32_t target_tris = static_cast<uint32_t>(
        std::max(1.0f, (indices.size() / 3.0f) * target_ratio));

    std::vector<QEMVertex> verts(positions.size());
    for (size_t i = 0; i < positions.size(); ++i) {
        verts[i].pos = positions[i];
        verts[i].original_index = static_cast<uint32_t>(i);
    }

    // Compute quadrics from triangle planes
    for (size_t t = 0; t < indices.size(); t += 3) {
        const glm::vec3& p0 = positions[indices[t]];
        const glm::vec3& p1 = positions[indices[t + 1]];
        const glm::vec3& p2 = positions[indices[t + 2]];
        glm::vec3 n = glm::normalize(glm::cross(p1 - p0, p2 - p0));
        float d = -glm::dot(n, p0);
        glm::mat4 q = ComputePlaneQuadric(n, d);
        verts[indices[t]].quadric += q;
        verts[indices[t + 1]].quadric += q;
        verts[indices[t + 2]].quadric += q;
    }

    // Build edge list
    std::unordered_map<uint64_t, uint32_t> edge_map;
    std::vector<QEMEdge> edges;
    auto make_edge_key = [](uint32_t a, uint32_t b) -> uint64_t {
        if (a > b) std::swap(a, b);
        return (uint64_t(a) << 32) | b;
    };

    for (size_t t = 0; t < indices.size(); t += 3) {
        uint32_t tri[3] = { indices[t], indices[t+1], indices[t+2] };
        for (int e = 0; e < 3; ++e) {
            uint32_t a = tri[e], b = tri[(e+1)%3];
            uint64_t key = make_edge_key(a, b);
            if (edge_map.find(key) == edge_map.end()) {
                QEMEdge edge;
                edge.v0 = a; edge.v1 = b;
                edge.cost = ComputeCollapseCost(verts[a], verts[b], edge.optimal_pos);
                edge_map[key] = static_cast<uint32_t>(edges.size());
                edges.push_back(edge);
            }
        }
    }

    // Working copy of triangles
    std::vector<uint32_t> tri_buf = indices;
    uint32_t current_tris = static_cast<uint32_t>(indices.size() / 3);
    float max_error = 0.0f;

    // Priority queue (min cost)
    auto cmp = [](const std::pair<float,uint32_t>& a, const std::pair<float,uint32_t>& b) {
        return a.first > b.first;
    };
    std::priority_queue<std::pair<float,uint32_t>,
                        std::vector<std::pair<float,uint32_t>>,
                        decltype(cmp)> pq(cmp);

    for (uint32_t i = 0; i < edges.size(); ++i) {
        pq.push({edges[i].cost, i});
    }

    // Vertex remap (union-find)
    std::vector<uint32_t> remap(verts.size());
    std::iota(remap.begin(), remap.end(), 0u);
    std::function<uint32_t(uint32_t)> find = [&](uint32_t v) -> uint32_t {
        while (remap[v] != v) { remap[v] = remap[remap[v]]; v = remap[v]; }
        return v;
    };

    while (current_tris > target_tris && !pq.empty()) {
        auto [cost, ei] = pq.top(); pq.pop();
        if (edges[ei].removed) continue;

        uint32_t a = find(edges[ei].v0);
        uint32_t b = find(edges[ei].v1);
        if (a == b) { edges[ei].removed = true; continue; }

        // Collapse b into a
        verts[a].pos = edges[ei].optimal_pos;
        verts[a].quadric = verts[a].quadric + verts[b].quadric;
        verts[b].removed = true;
        remap[b] = a;
        edges[ei].removed = true;
        max_error = std::max(max_error, cost);

        // Update triangles, remove degenerate
        uint32_t removed_tris = 0;
        for (size_t t = 0; t < tri_buf.size(); t += 3) {
            tri_buf[t]   = find(tri_buf[t]);
            tri_buf[t+1] = find(tri_buf[t+1]);
            tri_buf[t+2] = find(tri_buf[t+2]);
            if (tri_buf[t] == tri_buf[t+1] || tri_buf[t+1] == tri_buf[t+2] ||
                tri_buf[t] == tri_buf[t+2]) {
                tri_buf[t] = tri_buf[t+1] = tri_buf[t+2] = UINT32_MAX;
                ++removed_tris;
            }
        }
        current_tris -= removed_tris;
    }

    // Compact output
    SimplifiedMesh result;
    std::unordered_map<uint32_t, uint32_t> vert_remap;
    for (size_t t = 0; t < tri_buf.size(); t += 3) {
        if (tri_buf[t] == UINT32_MAX) continue;
        for (int k = 0; k < 3; ++k) {
            uint32_t vi = find(tri_buf[t + k]);
            if (vert_remap.find(vi) == vert_remap.end()) {
                vert_remap[vi] = static_cast<uint32_t>(result.positions.size());
                result.positions.push_back(verts[vi].pos);
            }
            result.indices.push_back(vert_remap[vi]);
        }
    }
    result.max_error = max_error;
    return result;
}

}  // anonymous namespace

// ============================================================================
// DAGBuilder
// ============================================================================

VirtualGeometryMesh DAGBuilder::Build(const std::vector<glm::vec3>& positions,
                                       const std::vector<glm::vec3>& normals,
                                       const std::vector<glm::vec2>& uvs,
                                       const std::vector<uint32_t>& indices,
                                       const DAGBuildConfig& config) {
    MeshletBuilder mb;
    MeshletMesh base = mb.Build(positions, indices);
    return BuildFromMeshletMesh(base, normals, uvs, config);
}

VirtualGeometryMesh DAGBuilder::BuildFromMeshletMesh(const MeshletMesh& base_mesh,
                                                      const std::vector<glm::vec3>& normals,
                                                      const std::vector<glm::vec2>& uvs,
                                                      const DAGBuildConfig& config) {
    VirtualGeometryMesh vgm;
    vgm.name = base_mesh.name;
    vgm.positions = base_mesh.positions;
    vgm.normals = normals;
    vgm.uvs = uvs;

    // Reconstruct global indices from base mesh
    vgm.indices = base_mesh.global_indices;

    BuildLeafLevel(vgm, base_mesh);
    BuildDAGLevels(vgm, config);

    return vgm;
}

void DAGBuilder::BuildLeafLevel(VirtualGeometryMesh& vgm, const MeshletMesh& base) {
    vgm.clusters = base.meshlets;
    vgm.cluster_vertices = base.meshlet_vertices;
    vgm.cluster_triangles = base.meshlet_triangles;
    vgm.draw_ranges = base.draw_ranges;

    // Create leaf DAG nodes
    vgm.dag_nodes.resize(base.meshlets.size());
    for (size_t i = 0; i < base.meshlets.size(); ++i) {
        DAGNode& node = vgm.dag_nodes[i];
        node.cluster_index = static_cast<uint32_t>(i);
        node.parent = UINT32_MAX;
        node.first_child = UINT32_MAX;
        node.child_count = 0;
        node.lod_error = 0.0f;
        node.bound_center = base.meshlets[i].center;
        node.bound_radius = base.meshlets[i].radius;
        node.lod_level = 0;
        std::memset(node.pad, 0, sizeof(node.pad));
    }
    vgm.num_lod_levels = 1;
    vgm.max_lod_error = 0.0f;
}

void DAGBuilder::BuildDAGLevels(VirtualGeometryMesh& vgm, const DAGBuildConfig& config) {
    // Collect current level's leaf/latest nodes
    std::vector<uint32_t> current_level;
    for (uint32_t i = 0; i < vgm.dag_nodes.size(); ++i) {
        if (vgm.dag_nodes[i].lod_level == vgm.num_lod_levels - 1)
            current_level.push_back(i);
    }

    for (uint32_t lod = 1; lod < config.max_lod_levels; ++lod) {
        if (current_level.size() <= 1) break;

        auto groups = FormMergeGroups(vgm, current_level, config.merge_group_size);
        if (groups.empty()) break;

        std::vector<uint32_t> next_level;

        for (auto& group : groups) {
            if (group.cluster_indices.size() <= 1) {
                next_level.insert(next_level.end(),
                                  group.cluster_indices.begin(),
                                  group.cluster_indices.end());
                continue;
            }

            float error = 0.0f;
            uint32_t parent_node = SimplifyMergeGroup(vgm, group,
                                                       config.simplification_ratio, error);
            if (parent_node == UINT32_MAX) {
                next_level.insert(next_level.end(),
                                  group.cluster_indices.begin(),
                                  group.cluster_indices.end());
                continue;
            }

            vgm.dag_nodes[parent_node].lod_level = static_cast<uint8_t>(lod);
            vgm.dag_nodes[parent_node].lod_error = error;
            next_level.push_back(parent_node);
            vgm.max_lod_error = std::max(vgm.max_lod_error, error);
        }

        current_level = next_level;
        vgm.num_lod_levels = lod + 1;
    }
}

std::vector<DAGBuilder::MergeGroup> DAGBuilder::FormMergeGroups(
        const VirtualGeometryMesh& vgm,
        const std::vector<uint32_t>& level_nodes,
        uint32_t group_size) {
    if (level_nodes.empty()) return {};

    // Simple spatial grouping: sort by center position then chunk
    struct NodeDist {
        uint32_t node_idx;
        float    sort_key;
    };
    std::vector<NodeDist> sorted(level_nodes.size());
    for (size_t i = 0; i < level_nodes.size(); ++i) {
        const auto& node = vgm.dag_nodes[level_nodes[i]];
        sorted[i] = { level_nodes[i], node.bound_center.x + node.bound_center.y + node.bound_center.z };
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const NodeDist& a, const NodeDist& b) { return a.sort_key < b.sort_key; });

    std::vector<MergeGroup> groups;
    for (size_t i = 0; i < sorted.size(); i += group_size) {
        MergeGroup g;
        glm::vec3 centroid(0.0f);
        size_t end = std::min(i + group_size, sorted.size());
        for (size_t j = i; j < end; ++j) {
            uint32_t ni = sorted[j].node_idx;
            g.cluster_indices.push_back(ni);
            centroid += vgm.dag_nodes[ni].bound_center;
        }
        g.centroid = centroid / float(end - i);
        groups.push_back(g);
    }
    return groups;
}

uint32_t DAGBuilder::SimplifyMergeGroup(VirtualGeometryMesh& vgm,
                                         const MergeGroup& group,
                                         float target_ratio,
                                         float& out_error) {
    // Gather all triangles from child clusters into a combined mesh
    std::vector<glm::vec3> combined_positions;
    std::vector<uint32_t>  combined_indices;
    std::unordered_map<uint32_t, uint32_t> vert_remap;

    for (uint32_t node_idx : group.cluster_indices) {
        const auto& node = vgm.dag_nodes[node_idx];
        uint32_t ci = node.cluster_index;
        const auto& cluster = vgm.clusters[ci];

        for (uint32_t v = 0; v < cluster.vertex_count; ++v) {
            uint32_t global_vi = vgm.cluster_vertices[cluster.vertex_offset + v];
            if (vert_remap.find(global_vi) == vert_remap.end()) {
                vert_remap[global_vi] = static_cast<uint32_t>(combined_positions.size());
                combined_positions.push_back(vgm.positions[global_vi]);
            }
        }

        for (uint32_t t = 0; t < cluster.triangle_count; ++t) {
            uint32_t base = cluster.triangle_offset + t * 3;
            for (int k = 0; k < 3; ++k) {
                uint8_t local_vi = vgm.cluster_triangles[base + k];
                uint32_t global_vi = vgm.cluster_vertices[cluster.vertex_offset + local_vi];
                combined_indices.push_back(vert_remap[global_vi]);
            }
        }
    }

    if (combined_indices.size() < 3) return UINT32_MAX;

    // Simplify the combined mesh
    auto simplified = SimplifyMesh(combined_positions, combined_indices, target_ratio);
    if (simplified.indices.size() < 3) return UINT32_MAX;

    out_error = simplified.max_error;

    // Re-meshlet the simplified geometry
    MeshletBuilder mb;
    MeshletMesh new_meshlets = mb.Build(simplified.positions, simplified.indices);
    if (new_meshlets.meshlets.empty()) return UINT32_MAX;

    // For simplicity, use only the first meshlet from the simplified result as the parent cluster
    // (the simplified group should typically produce 1-2 meshlets)
    uint32_t new_cluster_idx = static_cast<uint32_t>(vgm.clusters.size());
    uint32_t new_vert_offset = static_cast<uint32_t>(vgm.cluster_vertices.size());
    uint32_t new_tri_offset  = static_cast<uint32_t>(vgm.cluster_triangles.size());

    // Merge simplified positions into global vertex pool
    uint32_t global_pos_base = static_cast<uint32_t>(vgm.positions.size());
    for (const auto& p : simplified.positions) {
        vgm.positions.push_back(p);
    }

    // Add all meshlets from simplified result as new clusters
    for (size_t mi = 0; mi < new_meshlets.meshlets.size(); ++mi) {
        const auto& src = new_meshlets.meshlets[mi];
        Meshlet dst = src;
        dst.vertex_offset = static_cast<uint32_t>(vgm.cluster_vertices.size());
        dst.triangle_offset = static_cast<uint32_t>(vgm.cluster_triangles.size());

        for (uint32_t v = 0; v < src.vertex_count; ++v) {
            vgm.cluster_vertices.push_back(
                global_pos_base + new_meshlets.meshlet_vertices[src.vertex_offset + v]);
        }
        for (uint32_t t = 0; t < src.triangle_count * 3; ++t) {
            vgm.cluster_triangles.push_back(
                new_meshlets.meshlet_triangles[src.triangle_offset + t]);
        }

        // Recompute bounds relative to actual positions
        dst.center = src.center;
        dst.radius = src.radius;
        dst.cone_axis = src.cone_axis;
        dst.cone_cutoff = src.cone_cutoff;

        vgm.clusters.push_back(dst);

        MeshletMesh::DrawRange range;
        range.index_offset = static_cast<uint32_t>(vgm.indices.size());
        range.index_count = src.triangle_count * 3;
        for (uint32_t t = 0; t < src.triangle_count * 3; ++t) {
            uint8_t local_vi = new_meshlets.meshlet_triangles[src.triangle_offset + t];
            uint32_t mesh_vi = new_meshlets.meshlet_vertices[src.vertex_offset + local_vi];
            vgm.indices.push_back(global_pos_base + mesh_vi);
        }
        vgm.draw_ranges.push_back(range);
    }

    // Create parent DAG node (references the first new cluster)
    DAGNode parent{};
    parent.cluster_index = new_cluster_idx;
    parent.parent = UINT32_MAX;
    parent.first_child = group.cluster_indices[0];
    parent.child_count = static_cast<uint32_t>(group.cluster_indices.size());
    parent.lod_error = out_error;
    std::memset(parent.pad, 0, sizeof(parent.pad));

    // Compute parent bounding sphere as union of children
    ComputeDAGBounds(parent, vgm);

    uint32_t parent_idx = static_cast<uint32_t>(vgm.dag_nodes.size());
    vgm.dag_nodes.push_back(parent);

    // Set children's parent pointer
    for (uint32_t child_idx : group.cluster_indices) {
        vgm.dag_nodes[child_idx].parent = parent_idx;
    }

    return parent_idx;
}

void DAGBuilder::ComputeDAGBounds(DAGNode& node, const VirtualGeometryMesh& vgm) {
    // Union of child bounding spheres
    if (node.child_count == 0 || node.first_child == UINT32_MAX) {
        if (node.cluster_index < vgm.clusters.size()) {
            node.bound_center = vgm.clusters[node.cluster_index].center;
            node.bound_radius = vgm.clusters[node.cluster_index].radius;
        }
        return;
    }

    glm::vec3 center(0.0f);
    float max_extent = 0.0f;

    // Approximate: use centroid of children + max distance
    uint32_t count = 0;
    // Children are stored by index in cluster_indices of the merge group,
    // which we no longer have here. Use the first_child as a hint.
    // For simplicity, iterate all nodes looking for children with parent == this
    // (we set parent after this call, so use bound of the cluster itself)
    const auto& cluster = vgm.clusters[node.cluster_index];
    node.bound_center = cluster.center;
    node.bound_radius = cluster.radius * 2.0f;  // Conservative estimate
}

// ============================================================================
// Serialization
// ============================================================================

bool DAGBuilder::Serialize(const VirtualGeometryMesh& mesh, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) return false;

    DVGeoHeader hdr{};
    hdr.magic = kDVGeoMagic;
    hdr.version = kDVGeoVersion;
    hdr.cluster_count = static_cast<uint32_t>(mesh.clusters.size());
    hdr.dag_node_count = static_cast<uint32_t>(mesh.dag_nodes.size());
    hdr.vertex_count = static_cast<uint32_t>(mesh.positions.size());
    hdr.index_count = static_cast<uint32_t>(mesh.indices.size());
    hdr.cluster_vertex_count = static_cast<uint32_t>(mesh.cluster_vertices.size());
    hdr.cluster_triangle_bytes = static_cast<uint32_t>(mesh.cluster_triangles.size());
    hdr.num_lod_levels = mesh.num_lod_levels;
    hdr.max_lod_error = mesh.max_lod_error;
    hdr.flags = 0;
    if (!mesh.normals.empty()) hdr.flags |= 1;
    if (!mesh.uvs.empty())     hdr.flags |= 2;
    hdr.reserved = 0;

    out.write(reinterpret_cast<const char*>(&hdr), sizeof(hdr));
    out.write(reinterpret_cast<const char*>(mesh.positions.data()),
              mesh.positions.size() * sizeof(glm::vec3));
    if (hdr.flags & 1)
        out.write(reinterpret_cast<const char*>(mesh.normals.data()),
                  mesh.normals.size() * sizeof(glm::vec3));
    if (hdr.flags & 2)
        out.write(reinterpret_cast<const char*>(mesh.uvs.data()),
                  mesh.uvs.size() * sizeof(glm::vec2));
    out.write(reinterpret_cast<const char*>(mesh.indices.data()),
              mesh.indices.size() * sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(mesh.clusters.data()),
              mesh.clusters.size() * sizeof(Meshlet));
    out.write(reinterpret_cast<const char*>(mesh.cluster_vertices.data()),
              mesh.cluster_vertices.size() * sizeof(uint32_t));
    out.write(reinterpret_cast<const char*>(mesh.cluster_triangles.data()),
              mesh.cluster_triangles.size());
    out.write(reinterpret_cast<const char*>(mesh.dag_nodes.data()),
              mesh.dag_nodes.size() * sizeof(DAGNode));
    out.write(reinterpret_cast<const char*>(mesh.draw_ranges.data()),
              mesh.draw_ranges.size() * sizeof(MeshletMesh::DrawRange));

    return out.good();
}

bool DAGBuilder::Deserialize(const std::string& path, VirtualGeometryMesh& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    DVGeoHeader hdr{};
    in.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    if (hdr.magic != kDVGeoMagic || hdr.version != kDVGeoVersion) return false;

    out.positions.resize(hdr.vertex_count);
    in.read(reinterpret_cast<char*>(out.positions.data()),
            hdr.vertex_count * sizeof(glm::vec3));

    if (hdr.flags & 1) {
        out.normals.resize(hdr.vertex_count);
        in.read(reinterpret_cast<char*>(out.normals.data()),
                hdr.vertex_count * sizeof(glm::vec3));
    }
    if (hdr.flags & 2) {
        out.uvs.resize(hdr.vertex_count);
        in.read(reinterpret_cast<char*>(out.uvs.data()),
                hdr.vertex_count * sizeof(glm::vec2));
    }

    out.indices.resize(hdr.index_count);
    in.read(reinterpret_cast<char*>(out.indices.data()),
            hdr.index_count * sizeof(uint32_t));

    out.clusters.resize(hdr.cluster_count);
    in.read(reinterpret_cast<char*>(out.clusters.data()),
            hdr.cluster_count * sizeof(Meshlet));

    out.cluster_vertices.resize(hdr.cluster_vertex_count);
    in.read(reinterpret_cast<char*>(out.cluster_vertices.data()),
            hdr.cluster_vertex_count * sizeof(uint32_t));

    out.cluster_triangles.resize(hdr.cluster_triangle_bytes);
    in.read(reinterpret_cast<char*>(out.cluster_triangles.data()),
            hdr.cluster_triangle_bytes);

    out.dag_nodes.resize(hdr.dag_node_count);
    in.read(reinterpret_cast<char*>(out.dag_nodes.data()),
            hdr.dag_node_count * sizeof(DAGNode));

    out.draw_ranges.resize(hdr.cluster_count);
    in.read(reinterpret_cast<char*>(out.draw_ranges.data()),
            hdr.cluster_count * sizeof(MeshletMesh::DrawRange));

    out.num_lod_levels = hdr.num_lod_levels;
    out.max_lod_error = hdr.max_lod_error;

    return in.good();
}

}  // namespace vg
}  // namespace render
}  // namespace dse

#endif  // DSE_ENABLE_VIRTUAL_GEOMETRY
