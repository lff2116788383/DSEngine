/**
 * @file lightmap_baker.cpp
 * @brief GI 光照贴图烘焙器实现
 *
 * 核心流程：
 * 1. 构建 BVH 加速结构
 * 2. 对每个 lightmap texel：
 *    a. 通过 UV 找到对应的世界空间位置和法线（重心坐标插值）
 *    b. 直接光：对每个光源做可见性测试（shadow ray）
 *    c. 间接光：半球 cosine-weighted 随机采样，递归 N bounce
 *    d. AO：短距离半球采样遮蔽率
 * 3. 降噪（edge-aware bilateral filter）
 * 4. 打包输出
 */

#include "engine/render/gi/lightmap_baker.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <numeric>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>

namespace dse {
namespace render {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kInvPi = 1.0f / kPi;
constexpr float kEpsilon = 1e-6f;

// ─── BVH 加速结构 ─────────────────────────────────────────────────────────

struct AABB {
    glm::vec3 min_pt = glm::vec3(1e30f);
    glm::vec3 max_pt = glm::vec3(-1e30f);

    void Expand(const glm::vec3& p) {
        min_pt = glm::min(min_pt, p);
        max_pt = glm::max(max_pt, p);
    }
    void Expand(const AABB& b) {
        min_pt = glm::min(min_pt, b.min_pt);
        max_pt = glm::max(max_pt, b.max_pt);
    }
    glm::vec3 Center() const { return (min_pt + max_pt) * 0.5f; }
    float SurfaceArea() const {
        glm::vec3 d = max_pt - min_pt;
        return 2.0f * (d.x * d.y + d.y * d.z + d.z * d.x);
    }
};

struct Ray {
    glm::vec3 origin;
    glm::vec3 direction;
    float t_max = 1e30f;
};

struct HitInfo {
    float t = 1e30f;
    uint32_t tri_index = UINT32_MAX;
    float u = 0.0f, v = 0.0f;  // barycentric
    bool hit = false;
};

// Moller-Trumbore 射线-三角形相交
bool RayTriangle(const Ray& ray, const glm::vec3& v0, const glm::vec3& v1, const glm::vec3& v2,
                 float& t, float& u, float& v) {
    glm::vec3 e1 = v1 - v0;
    glm::vec3 e2 = v2 - v0;
    glm::vec3 h = glm::cross(ray.direction, e2);
    float a = glm::dot(e1, h);
    if (std::abs(a) < kEpsilon) return false;

    float f = 1.0f / a;
    glm::vec3 s = ray.origin - v0;
    u = f * glm::dot(s, h);
    if (u < 0.0f || u > 1.0f) return false;

    glm::vec3 q = glm::cross(s, e1);
    v = f * glm::dot(ray.direction, q);
    if (v < 0.0f || u + v > 1.0f) return false;

    t = f * glm::dot(e2, q);
    return t > kEpsilon && t < ray.t_max;
}

struct BvhNode {
    AABB bounds;
    uint32_t left = 0;      // 左子节点索引 (0=leaf 无子节点)
    uint32_t right = 0;     // 右子节点索引
    uint32_t first_tri = 0; // 叶节点：三角形列表起始
    uint32_t tri_count = 0; // 叶节点：三角形数量
    bool IsLeaf() const { return tri_count > 0; }
};

class BVH {
public:
    void Build(const std::vector<BakeTriangle>& triangles) {
        tris_ = &triangles;
        uint32_t n = static_cast<uint32_t>(triangles.size());
        tri_indices_.resize(n);
        std::iota(tri_indices_.begin(), tri_indices_.end(), 0u);

        nodes_.reserve(n * 2);
        nodes_.push_back({}); // root at index 0

        AABB root_bounds;
        for (uint32_t i = 0; i < n; ++i) {
            root_bounds.Expand(triangles[i].v0);
            root_bounds.Expand(triangles[i].v1);
            root_bounds.Expand(triangles[i].v2);
        }
        nodes_[0].bounds = root_bounds;
        nodes_[0].first_tri = 0;
        nodes_[0].tri_count = n;

        Subdivide(0);
    }

    HitInfo Trace(const Ray& ray) const {
        HitInfo hit;
        TraceNode(ray, 0, hit);
        return hit;
    }

    bool Occluded(const Ray& ray) const {
        return OccludedNode(ray, 0);
    }

private:
    const std::vector<BakeTriangle>* tris_ = nullptr;
    std::vector<BvhNode> nodes_;
    std::vector<uint32_t> tri_indices_;

    bool IntersectAABB(const Ray& ray, const AABB& box) const {
        glm::vec3 inv_dir = 1.0f / (ray.direction + glm::vec3(kEpsilon));
        glm::vec3 t0 = (box.min_pt - ray.origin) * inv_dir;
        glm::vec3 t1 = (box.max_pt - ray.origin) * inv_dir;
        glm::vec3 tmin = glm::min(t0, t1);
        glm::vec3 tmax = glm::max(t0, t1);
        float enter = std::max({tmin.x, tmin.y, tmin.z});
        float exit = std::min({tmax.x, tmax.y, tmax.z});
        return enter <= exit && exit >= 0.0f;
    }

    void TraceNode(const Ray& ray, uint32_t node_idx, HitInfo& hit) const {
        const BvhNode& node = nodes_[node_idx];
        if (!IntersectAABB(ray, node.bounds)) return;

        if (node.IsLeaf()) {
            for (uint32_t i = 0; i < node.tri_count; ++i) {
                uint32_t ti = tri_indices_[node.first_tri + i];
                const auto& tri = (*tris_)[ti];
                float t, u, v;
                Ray test_ray = ray;
                test_ray.t_max = hit.t;
                if (RayTriangle(test_ray, tri.v0, tri.v1, tri.v2, t, u, v)) {
                    if (t < hit.t) {
                        hit.t = t;
                        hit.tri_index = ti;
                        hit.u = u;
                        hit.v = v;
                        hit.hit = true;
                    }
                }
            }
            return;
        }

        TraceNode(ray, node.left, hit);
        TraceNode(ray, node.right, hit);
    }

    bool OccludedNode(const Ray& ray, uint32_t node_idx) const {
        const BvhNode& node = nodes_[node_idx];
        if (!IntersectAABB(ray, node.bounds)) return false;

        if (node.IsLeaf()) {
            for (uint32_t i = 0; i < node.tri_count; ++i) {
                uint32_t ti = tri_indices_[node.first_tri + i];
                const auto& tri = (*tris_)[ti];
                float t, u, v;
                if (RayTriangle(ray, tri.v0, tri.v1, tri.v2, t, u, v)) return true;
            }
            return false;
        }

        return OccludedNode(ray, node.left) || OccludedNode(ray, node.right);
    }

    glm::vec3 TriCenter(uint32_t ti) const {
        const auto& t = (*tris_)[ti];
        return (t.v0 + t.v1 + t.v2) / 3.0f;
    }

    void Subdivide(uint32_t node_idx) {
        BvhNode& node = nodes_[node_idx];
        if (node.tri_count <= 4) return; // 叶节点阈值

        // SAH 简化：按最长轴中位数分割
        int axis = 0;
        glm::vec3 extent = node.bounds.max_pt - node.bounds.min_pt;
        if (extent.y > extent.x && extent.y > extent.z) axis = 1;
        else if (extent.z > extent.x) axis = 2;

        float split = (node.bounds.min_pt[axis] + node.bounds.max_pt[axis]) * 0.5f;

        // 分区
        uint32_t start = node.first_tri;
        uint32_t end = start + node.tri_count;
        uint32_t mid = start;

        for (uint32_t i = start; i < end; ++i) {
            if (TriCenter(tri_indices_[i])[axis] < split) {
                std::swap(tri_indices_[i], tri_indices_[mid]);
                mid++;
            }
        }

        // 避免空分区
        if (mid == start || mid == end) {
            mid = (start + end) / 2;
        }

        // 创建子节点
        uint32_t left_idx = static_cast<uint32_t>(nodes_.size());
        nodes_.push_back({});
        uint32_t right_idx = static_cast<uint32_t>(nodes_.size());
        nodes_.push_back({});

        auto& left = nodes_[left_idx];
        auto& right = nodes_[right_idx];

        left.first_tri = start;
        left.tri_count = mid - start;
        right.first_tri = mid;
        right.tri_count = end - mid;

        // 计算子节点 AABB
        for (uint32_t i = start; i < mid; ++i) {
            const auto& t = (*tris_)[tri_indices_[i]];
            left.bounds.Expand(t.v0); left.bounds.Expand(t.v1); left.bounds.Expand(t.v2);
        }
        for (uint32_t i = mid; i < end; ++i) {
            const auto& t = (*tris_)[tri_indices_[i]];
            right.bounds.Expand(t.v0); right.bounds.Expand(t.v1); right.bounds.Expand(t.v2);
        }

        // 转为内节点
        // 注意：这里要重新取引用因为 nodes_ 可能 realloc
        nodes_[node_idx].left = left_idx;
        nodes_[node_idx].right = right_idx;
        nodes_[node_idx].tri_count = 0;

        Subdivide(left_idx);
        Subdivide(right_idx);
    }
};

// ─── 采样工具 ─────────────────────────────────────────────────────────────

// Cosine-weighted 半球采样
glm::vec3 CosineHemisphere(float r1, float r2) {
    float phi = 2.0f * kPi * r1;
    float cos_theta = std::sqrt(r2);
    float sin_theta = std::sqrt(1.0f - r2);
    return glm::vec3(std::cos(phi) * sin_theta, cos_theta, std::sin(phi) * sin_theta);
}

// 构建 TBN 从法线
void BuildTBN(const glm::vec3& n, glm::vec3& tangent, glm::vec3& bitangent) {
    glm::vec3 up = (std::abs(n.y) < 0.999f) ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
    tangent = glm::normalize(glm::cross(up, n));
    bitangent = glm::cross(n, tangent);
}

// 局部→世界半球方向
glm::vec3 LocalToWorld(const glm::vec3& local, const glm::vec3& n) {
    glm::vec3 t, b;
    BuildTBN(n, t, b);
    return local.x * t + local.y * n + local.z * b;
}

// ─── 直接光计算 ─────────────────────────────────────────────────────────

glm::vec3 EvaluateDirectLight(const BakeScene& scene, const BVH& bvh,
                               const glm::vec3& pos, const glm::vec3& normal, float bias) {
    glm::vec3 irradiance(0.0f);

    for (const auto& light : scene.lights) {
        glm::vec3 L;
        float attenuation = 1.0f;
        float max_dist = 1e30f;

        if (light.type == BakeLight::Type::Directional) {
            L = -light.direction;
            max_dist = 1e30f;
        } else if (light.type == BakeLight::Type::Point) {
            glm::vec3 to_light = light.position - pos;
            float dist = glm::length(to_light);
            if (dist > light.range) continue;
            L = to_light / dist;
            max_dist = dist;
            float falloff = 1.0f - dist / light.range;
            attenuation = falloff * falloff;
        } else { // Spot
            glm::vec3 to_light = light.position - pos;
            float dist = glm::length(to_light);
            if (dist > light.range) continue;
            L = to_light / dist;
            max_dist = dist;
            float cos_angle = glm::dot(-L, light.direction);
            float cos_outer = std::cos(light.spot_angle * kPi / 180.0f);
            if (cos_angle < cos_outer) continue;
            float falloff = 1.0f - dist / light.range;
            attenuation = falloff * falloff * ((cos_angle - cos_outer) / (1.0f - cos_outer));
        }

        float NdotL = glm::dot(normal, L);
        if (NdotL <= 0.0f) continue;

        // Shadow ray
        Ray shadow_ray;
        shadow_ray.origin = pos + normal * bias;
        shadow_ray.direction = L;
        shadow_ray.t_max = max_dist - bias * 2.0f;

        if (!bvh.Occluded(shadow_ray)) {
            irradiance += light.color * light.intensity * NdotL * attenuation;
        }
    }

    return irradiance;
}

// ─── 间接光追踪 ─────────────────────────────────────────────────────────

glm::vec3 TraceIndirect(const BakeScene& scene, const BVH& bvh,
                         const glm::vec3& pos, const glm::vec3& normal,
                         uint32_t bounces, float bias, std::mt19937& rng) {
    if (bounces == 0) return glm::vec3(0.0f);

    std::uniform_real_distribution<float> dist(0.0f, 1.0f);

    glm::vec3 local_dir = CosineHemisphere(dist(rng), dist(rng));
    glm::vec3 world_dir = LocalToWorld(local_dir, normal);

    Ray ray;
    ray.origin = pos + normal * bias;
    ray.direction = world_dir;

    HitInfo hit = bvh.Trace(ray);
    if (!hit.hit) {
        return scene.ambient;
    }

    const auto& tri = scene.triangles[hit.tri_index];
    float w = 1.0f - hit.u - hit.v;
    glm::vec3 hit_pos = tri.v0 * w + tri.v1 * hit.u + tri.v2 * hit.v;
    glm::vec3 hit_normal = glm::normalize(tri.n0 * w + tri.n1 * hit.u + tri.n2 * hit.v);

    // 直接光 at hit point
    glm::vec3 direct = EvaluateDirectLight(scene, bvh, hit_pos, hit_normal, bias);

    // 递归间接光
    glm::vec3 indirect = TraceIndirect(scene, bvh, hit_pos, hit_normal, bounces - 1, bias, rng);

    return tri.albedo * (direct + indirect);
}

} // anonymous namespace

// ─── LightmapBaker 公开 API ─────────────────────────────────────────────

LightmapResult LightmapBaker::Bake(const BakeScene& scene, const LightmapBakeConfig& config,
                                    BakeProgressCallback progress_cb) {
    LightmapResult result;
    result.width = config.resolution;
    result.height = config.resolution;
    uint32_t total_texels = config.resolution * config.resolution;
    result.irradiance.resize(total_texels, glm::vec3(0.0f));
    if (config.bake_ao) result.ao.resize(total_texels, 1.0f);

    if (scene.triangles.empty()) {
        result.success = true;
        return result;
    }

    // 构建 BVH
    BVH bvh;
    bvh.Build(scene.triangles);

    // 构建 UV→位置/法线 映射：对每个 texel 找对应的三角形和重心坐标
    // 使用光栅化方式填充 texel
    struct TexelInfo {
        glm::vec3 position = glm::vec3(0.0f);
        glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);
        glm::vec3 albedo = glm::vec3(0.8f);
        bool valid = false;
    };
    std::vector<TexelInfo> texel_map(total_texels);

    // UV 光栅化：将三角形在 UV 空间光栅化到 texel_map
    auto RasterizeUV = [&](const BakeTriangle& tri) {
        // 将 UV 坐标映射到 texel 空间
        glm::vec2 uv0 = tri.uv0 * static_cast<float>(config.resolution);
        glm::vec2 uv1 = tri.uv1 * static_cast<float>(config.resolution);
        glm::vec2 uv2 = tri.uv2 * static_cast<float>(config.resolution);

        // AABB
        int min_x = std::max(0, static_cast<int>(std::floor(std::min({uv0.x, uv1.x, uv2.x}))));
        int max_x = std::min(static_cast<int>(config.resolution) - 1,
                            static_cast<int>(std::ceil(std::max({uv0.x, uv1.x, uv2.x}))));
        int min_y = std::max(0, static_cast<int>(std::floor(std::min({uv0.y, uv1.y, uv2.y}))));
        int max_y = std::min(static_cast<int>(config.resolution) - 1,
                            static_cast<int>(std::ceil(std::max({uv0.y, uv1.y, uv2.y}))));

        for (int y = min_y; y <= max_y; ++y) {
            for (int x = min_x; x <= max_x; ++x) {
                glm::vec2 p(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);

                // 重心坐标
                glm::vec2 e0 = uv1 - uv0, e1 = uv2 - uv0, e2 = p - uv0;
                float d00 = glm::dot(e0, e0), d01 = glm::dot(e0, e1), d11 = glm::dot(e1, e1);
                float d20 = glm::dot(e2, e0), d21 = glm::dot(e2, e1);
                float denom = d00 * d11 - d01 * d01;
                if (std::abs(denom) < kEpsilon) continue;
                float inv_denom = 1.0f / denom;
                float bv = (d11 * d20 - d01 * d21) * inv_denom;
                float bw = (d00 * d21 - d01 * d20) * inv_denom;
                float bu = 1.0f - bv - bw;

                if (bu < -0.01f || bv < -0.01f || bw < -0.01f) continue;

                uint32_t idx = y * config.resolution + x;
                auto& texel = texel_map[idx];
                texel.position = tri.v0 * bu + tri.v1 * bv + tri.v2 * bw;
                texel.normal = glm::normalize(tri.n0 * bu + tri.n1 * bv + tri.n2 * bw);
                texel.albedo = tri.albedo;
                texel.valid = true;
            }
        }
    };

    for (const auto& tri : scene.triangles) {
        RasterizeUV(tri);
    }

    // 多线程烘焙
    std::atomic<uint32_t> progress_counter{0};
    uint32_t valid_count = 0;
    for (auto& t : texel_map) { if (t.valid) valid_count++; }

    uint32_t num_threads = std::max(1u, std::thread::hardware_concurrency());
    std::vector<std::thread> threads;
    uint32_t chunk = (total_texels + num_threads - 1) / num_threads;

    for (uint32_t t = 0; t < num_threads; ++t) {
        uint32_t start = t * chunk;
        uint32_t end = std::min(start + chunk, total_texels);

        threads.emplace_back([&, start, end, t]() {
            std::mt19937 rng(42 + t);
            std::uniform_real_distribution<float> dist(0.0f, 1.0f);

            for (uint32_t idx = start; idx < end; ++idx) {
                const auto& texel = texel_map[idx];
                if (!texel.valid) continue;

                glm::vec3 total_irradiance(0.0f);
                float total_ao = 0.0f;

                for (uint32_t s = 0; s < config.samples_per_texel; ++s) {
                    // 直接光
                    glm::vec3 direct = EvaluateDirectLight(scene, bvh,
                        texel.position, texel.normal, config.bias);

                    // 间接光
                    glm::vec3 indirect = TraceIndirect(scene, bvh,
                        texel.position, texel.normal, config.bounces, config.bias, rng);

                    total_irradiance += direct + indirect;

                    // AO
                    if (config.bake_ao) {
                        glm::vec3 ao_dir = LocalToWorld(CosineHemisphere(dist(rng), dist(rng)), texel.normal);
                        Ray ao_ray;
                        ao_ray.origin = texel.position + texel.normal * config.bias;
                        ao_ray.direction = ao_dir;
                        ao_ray.t_max = config.ao_radius;
                        if (bvh.Occluded(ao_ray)) {
                            total_ao += 1.0f;
                        }
                    }
                }

                float inv_samples = 1.0f / static_cast<float>(config.samples_per_texel);
                result.irradiance[idx] = texel.albedo * total_irradiance * inv_samples + scene.ambient;
                if (config.bake_ao) {
                    result.ao[idx] = 1.0f - total_ao * inv_samples;
                }

                uint32_t done = progress_counter.fetch_add(1);
                if (progress_cb && (done % 1000 == 0)) {
                    progress_cb(static_cast<float>(done) / static_cast<float>(valid_count));
                }
            }
        });
    }

    for (auto& th : threads) th.join();

    // 降噪
    if (config.denoise) {
        std::vector<glm::vec3> temp(total_texels);
        for (int iter = 0; iter < config.denoise_iterations; ++iter) {
            float sigma_s = config.denoise_sigma_spatial;
            float sigma_c = config.denoise_sigma_color;
            int radius = static_cast<int>(std::ceil(sigma_s * 2.0f));

            for (uint32_t y = 0; y < config.resolution; ++y) {
                for (uint32_t x = 0; x < config.resolution; ++x) {
                    uint32_t idx = y * config.resolution + x;
                    if (!texel_map[idx].valid) { temp[idx] = result.irradiance[idx]; continue; }

                    glm::vec3 center = result.irradiance[idx];
                    glm::vec3 sum(0.0f);
                    float weight_sum = 0.0f;

                    for (int dy = -radius; dy <= radius; ++dy) {
                        for (int dx = -radius; dx <= radius; ++dx) {
                            int nx = static_cast<int>(x) + dx;
                            int ny = static_cast<int>(y) + dy;
                            if (nx < 0 || nx >= static_cast<int>(config.resolution) ||
                                ny < 0 || ny >= static_cast<int>(config.resolution)) continue;

                            uint32_t nidx = ny * config.resolution + nx;
                            if (!texel_map[nidx].valid) continue;

                            float spatial_w = std::exp(-(dx*dx + dy*dy) / (2.0f * sigma_s * sigma_s));
                            glm::vec3 diff = result.irradiance[nidx] - center;
                            float color_dist = glm::dot(diff, diff);
                            float color_w = std::exp(-color_dist / (2.0f * sigma_c * sigma_c));

                            float w = spatial_w * color_w;
                            sum += result.irradiance[nidx] * w;
                            weight_sum += w;
                        }
                    }

                    temp[idx] = (weight_sum > 0.0f) ? sum / weight_sum : center;
                }
            }
            result.irradiance = temp;
        }
    }

    result.success = true;
    if (progress_cb) progress_cb(1.0f);
    return result;
}

// ─── .dlightmap 文件格式 ────────────────────────────────────────────────
// Header: magic(4) + version(4) + width(4) + height(4) + flags(4) = 20 bytes
// Data: irradiance (width*height * 12 bytes = vec3 * float)
// If has_ao: ao (width*height * 4 bytes = float)

bool LightmapBaker::SaveToFile(const LightmapResult& result, const std::string& output_path) {
    std::ofstream out(output_path, std::ios::binary);
    if (!out) return false;

    char magic[4] = {'D', 'S', 'L', 'M'};
    uint32_t version = 1;
    uint32_t flags = result.ao.empty() ? 0u : 1u; // bit 0 = has_ao

    out.write(magic, 4);
    out.write(reinterpret_cast<const char*>(&version), 4);
    out.write(reinterpret_cast<const char*>(&result.width), 4);
    out.write(reinterpret_cast<const char*>(&result.height), 4);
    out.write(reinterpret_cast<const char*>(&flags), 4);

    // Irradiance data (RGB32F)
    out.write(reinterpret_cast<const char*>(result.irradiance.data()),
              result.irradiance.size() * sizeof(glm::vec3));

    // AO data
    if (!result.ao.empty()) {
        out.write(reinterpret_cast<const char*>(result.ao.data()),
                  result.ao.size() * sizeof(float));
    }

    return out.good();
}

bool LightmapBaker::LoadFromFile(const std::string& path, LightmapResult& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;

    char magic[4];
    in.read(magic, 4);
    if (magic[0] != 'D' || magic[1] != 'S' || magic[2] != 'L' || magic[3] != 'M') return false;

    uint32_t version, flags;
    in.read(reinterpret_cast<char*>(&version), 4);
    in.read(reinterpret_cast<char*>(&out.width), 4);
    in.read(reinterpret_cast<char*>(&out.height), 4);
    in.read(reinterpret_cast<char*>(&flags), 4);

    if (version != 1) return false;

    uint32_t total = out.width * out.height;
    out.irradiance.resize(total);
    in.read(reinterpret_cast<char*>(out.irradiance.data()), total * sizeof(glm::vec3));

    if (flags & 1) {
        out.ao.resize(total);
        in.read(reinterpret_cast<char*>(out.ao.data()), total * sizeof(float));
    }

    out.success = in.good();
    return out.success;
}

} // namespace render
} // namespace dse
