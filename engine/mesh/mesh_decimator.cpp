/**
 * @file mesh_decimator.cpp
 * @brief QEM 网格减面器实现
 *
 * 算法流程：
 * 1. 构建连通性（vertex→triangles, edge→faces）
 * 2. 为每个顶点计算初始 Quadric（所有邻接面的平面方程 Q 矩阵之和）
 * 3. 为每条边计算折叠代价（新顶点位置使 Q1+Q2 误差最小化）
 * 4. 用最小堆选代价最小的边 → 折叠 → 更新邻居的代价
 * 5. 重复直到达到目标三角形数 / 误差阈值
 */

#include "engine/mesh/mesh_decimator.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <limits>
#include <numeric>
#include <queue>
#include <unordered_map>
#include <unordered_set>

namespace dse {
namespace mesh {

namespace {

// ─── Quadric 4x4 对称矩阵（存储上三角 10 个分量）─────────────────────────
struct Quadric {
    // 对称 4x4: a00 a01 a02 a03 / a11 a12 a13 / a22 a23 / a33
    double a[10] = {};

    Quadric() = default;

    // 从平面方程 ax+by+cz+d=0 构造 Q = pp^T (p=[a,b,c,d])
    Quadric(double a_, double b_, double c_, double d_) {
        a[0] = a_ * a_;  a[1] = a_ * b_;  a[2] = a_ * c_;  a[3] = a_ * d_;
        a[4] = b_ * b_;  a[5] = b_ * c_;  a[6] = b_ * d_;
        a[7] = c_ * c_;  a[8] = c_ * d_;
        a[9] = d_ * d_;
    }

    Quadric& operator+=(const Quadric& o) {
        for (int i = 0; i < 10; ++i) a[i] += o.a[i];
        return *this;
    }

    Quadric operator+(const Quadric& o) const {
        Quadric r = *this; r += o; return r;
    }

    Quadric& operator*=(double s) {
        for (int i = 0; i < 10; ++i) a[i] *= s;
        return *this;
    }

    // 计算 v^T Q v（v 为 homogeneous [x,y,z,1]）
    double Evaluate(double x, double y, double z) const {
        return a[0]*x*x + 2*a[1]*x*y + 2*a[2]*x*z + 2*a[3]*x
             + a[4]*y*y + 2*a[5]*y*z + 2*a[6]*y
             + a[7]*z*z + 2*a[8]*z
             + a[9];
    }

    // 尝试求最优位置（求解 3x3 线性系统 Av = b）
    // 返回 false 如果矩阵奇异（退化为线 / 点）
    bool OptimalPosition(double& ox, double& oy, double& oz) const {
        // 3x3 子矩阵
        double m00 = a[0], m01 = a[1], m02 = a[2];
        double m11 = a[4], m12 = a[5];
        double m22 = a[7];
        double b0 = -a[3], b1 = -a[6], b2 = -a[8];

        double det = m00*(m11*m22 - m12*m12) - m01*(m01*m22 - m12*m02) + m02*(m01*m12 - m11*m02);
        if (std::abs(det) < 1e-15) return false;

        double inv_det = 1.0 / det;
        ox = (b0*(m11*m22 - m12*m12) + b1*(m02*m12 - m01*m22) + b2*(m01*m12 - m02*m11)) * inv_det;
        oy = (b0*(m12*m02 - m01*m22) + b1*(m00*m22 - m02*m02) + b2*(m02*m01 - m00*m12)) * inv_det;
        oz = (b0*(m01*m12 - m11*m02) + b1*(m01*m02 - m00*m12) + b2*(m00*m11 - m01*m01)) * inv_det;
        return true;
    }
};

// ─── 内部数据结构 ─────────────────────────────────────────────────────────
struct Vertex {
    glm::vec3 pos{0.0f};
    glm::vec3 normal{0.0f, 1.0f, 0.0f};
    glm::vec2 uv{0.0f};
    glm::vec4 color{1.0f};
    Quadric q;
    bool locked = false;
    bool removed = false;
    std::vector<uint32_t> triangles;  // 引用的三角形索引
};

struct Triangle {
    uint32_t v[3] = {};
    bool removed = false;
    glm::vec3 normal{0.0f};
};

struct EdgeCollapse {
    uint32_t v0 = 0, v1 = 0;
    glm::vec3 optimal_pos{0.0f};
    double cost = 0.0;
    uint32_t generation = 0;  // 用于堆惰性删除
};

struct EdgeCollapseCompare {
    bool operator()(const EdgeCollapse& a, const EdgeCollapse& b) const {
        return a.cost > b.cost;  // min-heap
    }
};

// 无序边 key
struct EdgeKey {
    uint32_t v0, v1;
    EdgeKey(uint32_t a, uint32_t b) : v0(std::min(a, b)), v1(std::max(a, b)) {}
    bool operator==(const EdgeKey& o) const { return v0 == o.v0 && v1 == o.v1; }
};

struct EdgeKeyHash {
    size_t operator()(const EdgeKey& e) const {
        size_t h = std::hash<uint32_t>{}(e.v0);
        h ^= std::hash<uint32_t>{}(e.v1) + 0x9e3779b9 + (h << 6) + (h >> 2);
        return h;
    }
};

// ─── 实现 ─────────────────────────────────────────────────────────────────

class QemDecimator {
public:
    QemDecimator(const DecimationInput& input, const DecimationConfig& config)
        : config_(config) {
        BuildMesh(input);
    }

    DecimationResult Run() {
        DecimationResult result;
        result.original_triangle_count = active_triangle_count_;

        // 确定目标
        uint32_t target = 0;
        if (config_.target_triangle_count > 0) {
            target = config_.target_triangle_count;
        } else {
            target = static_cast<uint32_t>(active_triangle_count_ * config_.target_ratio);
        }
        target = std::max(target, 1u);

        // 初始化 Quadric
        ComputeVertexQuadrics();

        // 初始化边折叠堆
        InitEdgeHeap();

        // 迭代折叠
        while (active_triangle_count_ > target && !heap_.empty()) {
            EdgeCollapse top = heap_.top();
            heap_.pop();

            // 惰性删除：generation 检查
            if (vertices_[top.v0].removed || vertices_[top.v1].removed) continue;
            if (edge_generation_.count(EdgeKey(top.v0, top.v1)) &&
                edge_generation_[EdgeKey(top.v0, top.v1)] != top.generation) continue;

            // 误差阈值检查
            if (config_.max_error > 0.0f && top.cost > static_cast<double>(config_.max_error)) break;

            // 拓扑检查
            if (!CanCollapse(top.v0, top.v1, top.optimal_pos)) continue;

            // 执行折叠
            DoCollapse(top.v0, top.v1, top.optimal_pos);
        }

        // 紧凑输出
        CompactOutput(result);
        result.success = true;
        return result;
    }

private:
    DecimationConfig config_;
    std::vector<Vertex> vertices_;
    std::vector<Triangle> triangles_;
    uint32_t active_triangle_count_ = 0;

    std::priority_queue<EdgeCollapse, std::vector<EdgeCollapse>, EdgeCollapseCompare> heap_;
    std::unordered_map<EdgeKey, uint32_t, EdgeKeyHash> edge_generation_;
    uint32_t global_generation_ = 0;

    bool has_normals_ = false;
    bool has_uvs_ = false;
    bool has_colors_ = false;

    void BuildMesh(const DecimationInput& input) {
        has_normals_ = (input.normals != nullptr);
        has_uvs_ = (input.texcoords != nullptr);
        has_colors_ = (input.colors != nullptr);

        vertices_.resize(input.vertex_count);
        for (uint32_t i = 0; i < input.vertex_count; ++i) {
            vertices_[i].pos = input.positions[i];
            if (has_normals_) vertices_[i].normal = input.normals[i];
            if (has_uvs_) vertices_[i].uv = input.texcoords[i];
            if (has_colors_) vertices_[i].color = input.colors[i];
        }

        uint32_t tri_count = input.index_count / 3;
        triangles_.resize(tri_count);
        for (uint32_t t = 0; t < tri_count; ++t) {
            triangles_[t].v[0] = input.indices[t * 3 + 0];
            triangles_[t].v[1] = input.indices[t * 3 + 1];
            triangles_[t].v[2] = input.indices[t * 3 + 2];

            for (int j = 0; j < 3; ++j) {
                vertices_[triangles_[t].v[j]].triangles.push_back(t);
            }

            // 计算面法线
            const glm::vec3& p0 = vertices_[triangles_[t].v[0]].pos;
            const glm::vec3& p1 = vertices_[triangles_[t].v[1]].pos;
            const glm::vec3& p2 = vertices_[triangles_[t].v[2]].pos;
            glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
            float len = glm::length(fn);
            triangles_[t].normal = (len > 1e-8f) ? fn / len : glm::vec3(0, 1, 0);
        }
        active_triangle_count_ = tri_count;

        // 标记边界顶点和 UV 接缝顶点
        MarkBoundaryAndSeams();
    }

    void MarkBoundaryAndSeams() {
        // 边界检测：出现奇数次的边
        std::unordered_map<uint64_t, int> edge_count;
        auto make_edge = [](uint32_t a, uint32_t b) -> uint64_t {
            return (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
        };

        for (uint32_t t = 0; t < static_cast<uint32_t>(triangles_.size()); ++t) {
            if (triangles_[t].removed) continue;
            for (int j = 0; j < 3; ++j) {
                uint32_t a = triangles_[t].v[j];
                uint32_t b = triangles_[t].v[(j + 1) % 3];
                edge_count[make_edge(a, b)]++;
            }
        }

        for (auto& [edge, count] : edge_count) {
            if (count == 1) {
                uint32_t a = static_cast<uint32_t>(edge >> 32);
                uint32_t b = static_cast<uint32_t>(edge & 0xFFFFFFFF);
                if (config_.lock_boundary) {
                    vertices_[a].locked = true;
                    vertices_[b].locked = true;
                }
            }
        }
    }

    void ComputeVertexQuadrics() {
        for (uint32_t t = 0; t < static_cast<uint32_t>(triangles_.size()); ++t) {
            if (triangles_[t].removed) continue;
            const glm::vec3& n = triangles_[t].normal;
            const glm::vec3& p = vertices_[triangles_[t].v[0]].pos;

            // 平面 ax+by+cz+d=0
            double a = n.x, b = n.y, c = n.z;
            double d = -(a * p.x + b * p.y + c * p.z);
            Quadric q(a, b, c, d);

            for (int j = 0; j < 3; ++j) {
                vertices_[triangles_[t].v[j]].q += q;
            }
        }

        // 边界惩罚 quadric
        if (config_.boundary_weight > 1.0f) {
            AddBoundaryPenalties();
        }
    }

    void AddBoundaryPenalties() {
        std::unordered_map<uint64_t, int> edge_count;
        auto make_edge = [](uint32_t a, uint32_t b) -> uint64_t {
            return (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
        };

        for (uint32_t t = 0; t < static_cast<uint32_t>(triangles_.size()); ++t) {
            if (triangles_[t].removed) continue;
            for (int j = 0; j < 3; ++j) {
                uint32_t a = triangles_[t].v[j];
                uint32_t b = triangles_[t].v[(j + 1) % 3];
                edge_count[make_edge(a, b)]++;
            }
        }

        for (auto& [edge, count] : edge_count) {
            if (count != 1) continue;  // 非边界边
            uint32_t a = static_cast<uint32_t>(edge >> 32);
            uint32_t b = static_cast<uint32_t>(edge & 0xFFFFFFFF);

            // 构造边界约束平面（垂直于边和邻接面法线）
            glm::vec3 edge_dir = glm::normalize(vertices_[b].pos - vertices_[a].pos);
            // 找邻接面法线
            glm::vec3 face_n(0, 1, 0);
            for (uint32_t tri_idx : vertices_[a].triangles) {
                if (triangles_[tri_idx].removed) continue;
                auto& tri = triangles_[tri_idx];
                for (int j = 0; j < 3; ++j) {
                    if ((tri.v[j] == a && tri.v[(j+1)%3] == b) ||
                        (tri.v[j] == b && tri.v[(j+1)%3] == a)) {
                        face_n = tri.normal;
                        goto found;
                    }
                }
            }
            found:
            glm::vec3 boundary_n = glm::cross(edge_dir, face_n);
            float len = glm::length(boundary_n);
            if (len < 1e-8f) continue;
            boundary_n /= len;

            double bn_a = boundary_n.x, bn_b = boundary_n.y, bn_c = boundary_n.z;
            double bn_d = -(bn_a * vertices_[a].pos.x + bn_b * vertices_[a].pos.y + bn_c * vertices_[a].pos.z);
            Quadric penalty(bn_a, bn_b, bn_c, bn_d);
            penalty *= static_cast<double>(config_.boundary_weight);

            vertices_[a].q += penalty;
            vertices_[b].q += penalty;
        }
    }

    EdgeCollapse ComputeEdgeCost(uint32_t v0, uint32_t v1) {
        EdgeCollapse ec;
        ec.v0 = v0;
        ec.v1 = v1;
        ec.generation = ++global_generation_;
        edge_generation_[EdgeKey(v0, v1)] = ec.generation;

        Quadric combined = vertices_[v0].q + vertices_[v1].q;

        // UV 接缝惩罚
        double seam_penalty = 0.0;
        if (config_.protect_uv_seams && has_uvs_) {
            if (IsUvSeamEdge(v0, v1)) {
                seam_penalty = static_cast<double>(config_.seam_weight);
            }
        }

        // 尝试最优位置
        double ox, oy, oz;
        if (combined.OptimalPosition(ox, oy, oz)) {
            ec.optimal_pos = glm::vec3(static_cast<float>(ox), static_cast<float>(oy), static_cast<float>(oz));
            ec.cost = combined.Evaluate(ox, oy, oz);
        } else {
            // 退化：选 v0, v1, midpoint 中代价最小的
            glm::vec3 mid = (vertices_[v0].pos + vertices_[v1].pos) * 0.5f;
            double c0 = combined.Evaluate(vertices_[v0].pos.x, vertices_[v0].pos.y, vertices_[v0].pos.z);
            double c1 = combined.Evaluate(vertices_[v1].pos.x, vertices_[v1].pos.y, vertices_[v1].pos.z);
            double cm = combined.Evaluate(mid.x, mid.y, mid.z);
            if (c0 <= c1 && c0 <= cm) { ec.optimal_pos = vertices_[v0].pos; ec.cost = c0; }
            else if (c1 <= cm)         { ec.optimal_pos = vertices_[v1].pos; ec.cost = c1; }
            else                       { ec.optimal_pos = mid; ec.cost = cm; }
        }

        // 属性差异惩罚
        if (config_.attribute_weight > 0.0f && has_normals_) {
            float ndot = glm::dot(vertices_[v0].normal, vertices_[v1].normal);
            double attr_penalty = (1.0 - static_cast<double>(ndot)) * config_.attribute_weight;
            ec.cost += attr_penalty;
        }

        // 接缝惩罚乘进
        if (seam_penalty > 0.0) {
            ec.cost *= (1.0 + seam_penalty);
        }

        // 锁定顶点：无穷代价
        if (vertices_[v0].locked || vertices_[v1].locked) {
            ec.cost = std::numeric_limits<double>::max();
        }

        return ec;
    }

    bool IsUvSeamEdge(uint32_t v0, uint32_t v1) {
        if (!has_uvs_) return false;
        // 简单启发式：如果两个顶点的 UV 跨越不连续（分属不同 UV island），
        // 通过检查共享三角形中 UV 的连续性判定
        int shared_count = 0;
        for (uint32_t t : vertices_[v0].triangles) {
            if (triangles_[t].removed) continue;
            for (int j = 0; j < 3; ++j) {
                if (triangles_[t].v[j] == v1) { shared_count++; break; }
            }
        }
        // 边界边（只有一个共享面）视为接缝
        return shared_count <= 1;
    }

    void InitEdgeHeap() {
        std::unordered_set<uint64_t> visited_edges;
        auto make_key = [](uint32_t a, uint32_t b) -> uint64_t {
            return (static_cast<uint64_t>(std::min(a, b)) << 32) | std::max(a, b);
        };

        for (uint32_t t = 0; t < static_cast<uint32_t>(triangles_.size()); ++t) {
            if (triangles_[t].removed) continue;
            for (int j = 0; j < 3; ++j) {
                uint32_t a = triangles_[t].v[j];
                uint32_t b = triangles_[t].v[(j + 1) % 3];
                uint64_t key = make_key(a, b);
                if (visited_edges.count(key)) continue;
                visited_edges.insert(key);
                heap_.push(ComputeEdgeCost(a, b));
            }
        }
    }

    bool CanCollapse(uint32_t v0, uint32_t v1, const glm::vec3& new_pos) {
        // 翻转检测：检查折叠后 v0 的所有邻接面法线是否翻转
        for (uint32_t t : vertices_[v0].triangles) {
            if (triangles_[t].removed) continue;
            auto& tri = triangles_[t];
            // 跳过即将被删除的面（同时包含 v0 和 v1 的面）
            bool has_v1 = false;
            for (int j = 0; j < 3; ++j) {
                if (tri.v[j] == v1) { has_v1 = true; break; }
            }
            if (has_v1) continue;

            // 计算折叠后的新法线
            glm::vec3 p[3];
            for (int j = 0; j < 3; ++j) {
                p[j] = (tri.v[j] == v0) ? new_pos : vertices_[tri.v[j]].pos;
            }
            glm::vec3 new_n = glm::cross(p[1] - p[0], p[2] - p[0]);
            float len = glm::length(new_n);
            if (len < 1e-10f) return false;  // 退化三角形
            new_n /= len;

            if (glm::dot(new_n, tri.normal) < config_.normal_flip_threshold) {
                return false;  // 法线翻转
            }
        }

        // 对 v1 的邻接面也做同样检查
        for (uint32_t t : vertices_[v1].triangles) {
            if (triangles_[t].removed) continue;
            auto& tri = triangles_[t];
            bool has_v0 = false;
            for (int j = 0; j < 3; ++j) {
                if (tri.v[j] == v0) { has_v0 = true; break; }
            }
            if (has_v0) continue;

            glm::vec3 p[3];
            for (int j = 0; j < 3; ++j) {
                p[j] = (tri.v[j] == v1) ? new_pos : vertices_[tri.v[j]].pos;
            }
            glm::vec3 new_n = glm::cross(p[1] - p[0], p[2] - p[0]);
            float len = glm::length(new_n);
            if (len < 1e-10f) return false;
            new_n /= len;

            if (glm::dot(new_n, tri.normal) < config_.normal_flip_threshold) {
                return false;
            }
        }

        return true;
    }

    void DoCollapse(uint32_t v0, uint32_t v1, const glm::vec3& new_pos) {
        // v0 存活，v1 被删除；v0 移到 new_pos
        vertices_[v0].pos = new_pos;
        vertices_[v0].q += vertices_[v1].q;

        // 插值属性（简单取平均）
        if (has_normals_) {
            vertices_[v0].normal = glm::normalize(vertices_[v0].normal + vertices_[v1].normal);
        }
        if (has_uvs_) {
            vertices_[v0].uv = (vertices_[v0].uv + vertices_[v1].uv) * 0.5f;
        }
        if (has_colors_) {
            vertices_[v0].color = (vertices_[v0].color + vertices_[v1].color) * 0.5f;
        }

        // 删除共享面（包含 v0 和 v1 的面）
        for (uint32_t t : vertices_[v1].triangles) {
            if (triangles_[t].removed) continue;
            auto& tri = triangles_[t];

            bool has_v0 = false;
            for (int j = 0; j < 3; ++j) {
                if (tri.v[j] == v0) { has_v0 = true; break; }
            }

            if (has_v0) {
                // 这个三角形同时引用 v0 和 v1，退化了，删除
                tri.removed = true;
                active_triangle_count_--;
            } else {
                // 重映射 v1 → v0
                for (int j = 0; j < 3; ++j) {
                    if (tri.v[j] == v1) tri.v[j] = v0;
                }
                // 更新面法线
                const glm::vec3& p0 = vertices_[tri.v[0]].pos;
                const glm::vec3& p1 = vertices_[tri.v[1]].pos;
                const glm::vec3& p2 = vertices_[tri.v[2]].pos;
                glm::vec3 fn = glm::cross(p1 - p0, p2 - p0);
                float len = glm::length(fn);
                tri.normal = (len > 1e-8f) ? fn / len : glm::vec3(0, 1, 0);

                // v0 获得该三角形
                vertices_[v0].triangles.push_back(t);
            }
        }

        // 删除 v1
        vertices_[v1].removed = true;
        vertices_[v1].triangles.clear();

        // 去重 v0 的三角形列表
        auto& v0_tris = vertices_[v0].triangles;
        std::sort(v0_tris.begin(), v0_tris.end());
        v0_tris.erase(std::unique(v0_tris.begin(), v0_tris.end()), v0_tris.end());
        // 移除已删除的
        v0_tris.erase(std::remove_if(v0_tris.begin(), v0_tris.end(),
            [this](uint32_t t) { return triangles_[t].removed; }), v0_tris.end());

        // 更新 v0 的所有邻接边的代价
        std::unordered_set<uint32_t> neighbors;
        for (uint32_t t : v0_tris) {
            for (int j = 0; j < 3; ++j) {
                uint32_t nv = triangles_[t].v[j];
                if (nv != v0 && !vertices_[nv].removed) {
                    neighbors.insert(nv);
                }
            }
        }
        for (uint32_t nv : neighbors) {
            heap_.push(ComputeEdgeCost(v0, nv));
        }
    }

    void CompactOutput(DecimationResult& result) {
        // 建立旧→新顶点映射
        std::vector<uint32_t> remap(vertices_.size(), UINT32_MAX);
        uint32_t new_count = 0;

        for (uint32_t t = 0; t < static_cast<uint32_t>(triangles_.size()); ++t) {
            if (triangles_[t].removed) continue;
            for (int j = 0; j < 3; ++j) {
                uint32_t vi = triangles_[t].v[j];
                if (remap[vi] == UINT32_MAX) {
                    remap[vi] = new_count++;
                }
            }
        }

        result.positions.resize(new_count);
        if (has_normals_) result.normals.resize(new_count);
        if (has_uvs_) result.texcoords.resize(new_count);
        if (has_colors_) result.colors.resize(new_count);

        for (uint32_t i = 0; i < static_cast<uint32_t>(vertices_.size()); ++i) {
            if (remap[i] == UINT32_MAX) continue;
            uint32_t ni = remap[i];
            result.positions[ni] = vertices_[i].pos;
            if (has_normals_) result.normals[ni] = vertices_[i].normal;
            if (has_uvs_) result.texcoords[ni] = vertices_[i].uv;
            if (has_colors_) result.colors[ni] = vertices_[i].color;
        }

        for (uint32_t t = 0; t < static_cast<uint32_t>(triangles_.size()); ++t) {
            if (triangles_[t].removed) continue;
            result.indices.push_back(remap[triangles_[t].v[0]]);
            result.indices.push_back(remap[triangles_[t].v[1]]);
            result.indices.push_back(remap[triangles_[t].v[2]]);
        }

        result.result_triangle_count = active_triangle_count_;
    }
};

} // anonymous namespace

// ─── 公开 API ─────────────────────────────────────────────────────────────

DecimationResult MeshDecimator::Decimate(const DecimationInput& input, const DecimationConfig& config) {
    if (!input.positions || input.vertex_count == 0 || !input.indices || input.index_count < 3) {
        DecimationResult r;
        r.success = false;
        return r;
    }
    QemDecimator dec(input, config);
    return dec.Run();
}

LodGenerationResult MeshDecimator::GenerateLods(const DecimationInput& input, const LodGenerationConfig& config) {
    LodGenerationResult result;
    if (config.level_ratios.empty()) {
        result.success = false;
        return result;
    }

    result.levels.resize(config.level_ratios.size());

    for (size_t i = 0; i < config.level_ratios.size(); ++i) {
        DecimationConfig level_config = config.base_config;
        level_config.target_ratio = config.level_ratios[i];
        level_config.target_triangle_count = 0;  // 使用 ratio

        result.levels[i] = Decimate(input, level_config);
        if (!result.levels[i].success) {
            result.success = false;
            return result;
        }
    }

    result.success = true;
    return result;
}

} // namespace mesh
} // namespace dse
