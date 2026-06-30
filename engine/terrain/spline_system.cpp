/**
 * @file spline_system.cpp
 * @brief 样条系统实现 — Catmull-Rom 插值 + 道路/河流网格生成
 */

#include "engine/terrain/spline_system.h"
#include <cmath>
#include <algorithm>
#include <numeric>

namespace dse {
namespace terrain {

uint32_t SplineSystem::CreateSpline(const std::string& name) {
    uint32_t id = next_id_++;
    if (id >= id_to_index_.size()) id_to_index_.resize(id + 1, UINT32_MAX);
    id_to_index_[id] = static_cast<uint32_t>(splines_.size());
    splines_.push_back({name, {}, -1.0f, {}});
    return id;
}

void SplineSystem::DestroySpline(uint32_t spline_id) {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return;
    uint32_t idx = id_to_index_[spline_id];
    if (idx < splines_.size() - 1) {
        splines_[idx] = std::move(splines_.back());
        // Update id_to_index for the moved spline
        for (uint32_t i = 0; i < id_to_index_.size(); ++i) {
            if (id_to_index_[i] == static_cast<uint32_t>(splines_.size() - 1)) {
                id_to_index_[i] = idx;
                break;
            }
        }
    }
    splines_.pop_back();
    id_to_index_[spline_id] = UINT32_MAX;
}

void SplineSystem::AddPoint(uint32_t spline_id, const SplinePoint& point) {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return;
    auto& s = splines_[id_to_index_[spline_id]];
    s.points.push_back(point);
    s.InvalidateCache();
}

void SplineSystem::InsertPoint(uint32_t spline_id, uint32_t index, const SplinePoint& point) {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return;
    auto& s = splines_[id_to_index_[spline_id]];
    if (index > s.points.size()) index = static_cast<uint32_t>(s.points.size());
    s.points.insert(s.points.begin() + index, point);
    s.InvalidateCache();
}

void SplineSystem::RemovePoint(uint32_t spline_id, uint32_t index) {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return;
    auto& s = splines_[id_to_index_[spline_id]];
    if (index >= s.points.size()) return;
    s.points.erase(s.points.begin() + index);
    s.InvalidateCache();
}

void SplineSystem::SetPoint(uint32_t spline_id, uint32_t index, const SplinePoint& point) {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return;
    auto& s = splines_[id_to_index_[spline_id]];
    if (index >= s.points.size()) return;
    s.points[index] = point;
    s.InvalidateCache();
}

uint32_t SplineSystem::GetPointCount(uint32_t spline_id) const {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return 0;
    return static_cast<uint32_t>(splines_[id_to_index_[spline_id]].points.size());
}

SplinePoint SplineSystem::GetPoint(uint32_t spline_id, uint32_t index) const {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return {};
    const auto& s = splines_[id_to_index_[spline_id]];
    if (index >= s.points.size()) return {};
    return s.points[index];
}

glm::vec3 SplineSystem::CatmullRomInterp(const glm::vec3& p0, const glm::vec3& p1,
                                          const glm::vec3& p2, const glm::vec3& p3, float t) const {
    float t2 = t * t;
    float t3 = t2 * t;
    return 0.5f * ((2.0f * p1) +
                   (-p0 + p2) * t +
                   (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2 +
                   (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
}

void SplineSystem::EnsureLengthCache(const SplineData& spline) const {
    if (spline.cached_length >= 0.0f) return;
    int n = static_cast<int>(spline.points.size());
    if (n < 2) { spline.cached_length = 0.0f; return; }

    spline.segment_lengths.resize(n - 1);
    spline.cached_length = 0.0f;

    const int subdivisions = 16;
    for (int seg = 0; seg < n - 1; ++seg) {
        int i0 = std::max(0, seg - 1);
        int i1 = seg;
        int i2 = std::min(n - 1, seg + 1);
        int i3 = std::min(n - 1, seg + 2);

        float seg_len = 0.0f;
        glm::vec3 prev = spline.points[i1].position;
        for (int sub = 1; sub <= subdivisions; ++sub) {
            float t = static_cast<float>(sub) / subdivisions;
            glm::vec3 cur = CatmullRomInterp(spline.points[i0].position,
                                              spline.points[i1].position,
                                              spline.points[i2].position,
                                              spline.points[i3].position, t);
            seg_len += glm::length(cur - prev);
            prev = cur;
        }
        spline.segment_lengths[seg] = seg_len;
        spline.cached_length += seg_len;
    }
}

float SplineSystem::GetSplineLength(uint32_t spline_id) const {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return 0.0f;
    const auto& s = splines_[id_to_index_[spline_id]];
    EnsureLengthCache(s);
    return s.cached_length;
}

SplineSample SplineSystem::EvaluateAtParam(uint32_t spline_id, float t) const {
    SplineSample result{};
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return result;
    const auto& s = splines_[id_to_index_[spline_id]];
    int n = static_cast<int>(s.points.size());
    if (n < 2) {
        if (n == 1) { result.position = s.points[0].position; result.width = s.points[0].width; }
        return result;
    }

    t = std::max(0.0f, std::min(t, 1.0f));
    float scaled = t * (n - 1);
    int seg = std::min(static_cast<int>(scaled), n - 2);
    float local_t = scaled - seg;

    int i0 = std::max(0, seg - 1);
    int i1 = seg;
    int i2 = std::min(n - 1, seg + 1);
    int i3 = std::min(n - 1, seg + 2);

    result.position = CatmullRomInterp(s.points[i0].position, s.points[i1].position,
                                        s.points[i2].position, s.points[i3].position, local_t);

    // Tangent via finite difference
    float dt = 0.001f;
    glm::vec3 p_next = CatmullRomInterp(s.points[i0].position, s.points[i1].position,
                                          s.points[i2].position, s.points[i3].position,
                                          std::min(local_t + dt, 1.0f));
    glm::vec3 p_prev = CatmullRomInterp(s.points[i0].position, s.points[i1].position,
                                          s.points[i2].position, s.points[i3].position,
                                          std::max(local_t - dt, 0.0f));
    result.tangent = glm::normalize(p_next - p_prev);

    // Interpolate width and banking
    result.width = LerpFloat(s.points[i1].width, s.points[i2].width, local_t);
    result.banking = LerpFloat(s.points[i1].banking, s.points[i2].banking, local_t);

    // Up and normal
    glm::vec3 up_interp = glm::normalize(glm::mix(s.points[i1].up, s.points[i2].up, local_t));
    result.normal = glm::normalize(glm::cross(result.tangent, up_interp));
    result.up = glm::normalize(glm::cross(result.normal, result.tangent));

    EnsureLengthCache(s);
    result.distance = 0.0f;
    for (int i = 0; i < seg; ++i) result.distance += s.segment_lengths[i];
    result.distance += s.segment_lengths[seg] * local_t;

    return result;
}

SplineSample SplineSystem::EvaluateAtDistance(uint32_t spline_id, float distance) const {
    if (spline_id >= id_to_index_.size() || id_to_index_[spline_id] == UINT32_MAX) return {};
    const auto& s = splines_[id_to_index_[spline_id]];
    EnsureLengthCache(s);
    if (s.cached_length <= 0.0f) return EvaluateAtParam(spline_id, 0.0f);

    float t = std::max(0.0f, std::min(distance / s.cached_length, 1.0f));
    return EvaluateAtParam(spline_id, t);
}

std::vector<SplineSample> SplineSystem::SampleUniform(uint32_t spline_id, uint32_t count) const {
    std::vector<SplineSample> samples;
    if (count == 0) return samples;
    samples.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        float t = (count == 1) ? 0.0f : static_cast<float>(i) / (count - 1);
        samples.push_back(EvaluateAtParam(spline_id, t));
    }
    return samples;
}

SplineMesh SplineSystem::GenerateRoadMesh(uint32_t spline_id, const RoadConfig& config) const {
    SplineMesh mesh;
    float length = GetSplineLength(spline_id);
    if (length <= 0.0f) return mesh;

    int num_segments = std::max(1, static_cast<int>(std::ceil(length / config.segment_length)));
    int num_cross = config.width_segments + 1;
    int num_rows = num_segments + 1;

    mesh.vertices.reserve(num_rows * num_cross);
    mesh.indices.reserve(num_segments * config.width_segments * 6);

    for (int row = 0; row < num_rows; ++row) {
        float t = static_cast<float>(row) / num_segments;
        SplineSample sample = EvaluateAtParam(spline_id, t);

        for (int col = 0; col < num_cross; ++col) {
            float u = static_cast<float>(col) / config.width_segments;
            float offset = (u - 0.5f) * sample.width;

            // Apply banking rotation
            glm::vec3 right = sample.normal;
            glm::vec3 up = sample.up;
            if (std::abs(sample.banking) > 0.001f) {
                float rad = glm::radians(sample.banking);
                float c = std::cos(rad), s = std::sin(rad);
                glm::vec3 new_right = right * c + up * s;
                up = -right * s + up * c;
                right = new_right;
            }

            SplineMeshVertex vert;
            vert.position = sample.position + right * offset;

            // Conform to terrain
            if (config.conform_to_terrain && terrain_height_func_) {
                float terrain_h = terrain_height_func_(vert.position.x, vert.position.z);
                vert.position.y = terrain_h - config.embed_depth;
            }

            vert.normal = up;
            vert.uv = glm::vec2(u, sample.distance * config.uv_repeat);
            mesh.vertices.push_back(vert);
        }
    }

    // Generate indices
    for (int row = 0; row < num_segments; ++row) {
        for (int col = 0; col < config.width_segments; ++col) {
            uint32_t tl = row * num_cross + col;
            uint32_t tr = tl + 1;
            uint32_t bl = (row + 1) * num_cross + col;
            uint32_t br = bl + 1;

            mesh.indices.push_back(tl); mesh.indices.push_back(bl); mesh.indices.push_back(tr);
            mesh.indices.push_back(tr); mesh.indices.push_back(bl); mesh.indices.push_back(br);
        }
    }

    return mesh;
}

SplineMesh SplineSystem::GenerateRiverMesh(uint32_t spline_id, const RiverConfig& config) const {
    SplineMesh mesh;
    float length = GetSplineLength(spline_id);
    if (length <= 0.0f) return mesh;

    int num_segments = std::max(1, static_cast<int>(std::ceil(length / config.segment_length)));
    int num_cross = config.width_segments + 1;
    int num_rows = num_segments + 1;

    mesh.vertices.reserve(num_rows * num_cross);
    mesh.indices.reserve(num_segments * config.width_segments * 6);

    for (int row = 0; row < num_rows; ++row) {
        float t = static_cast<float>(row) / num_segments;
        SplineSample sample = EvaluateAtParam(spline_id, t);

        for (int col = 0; col < num_cross; ++col) {
            float u = static_cast<float>(col) / config.width_segments;
            float offset = (u - 0.5f) * sample.width;

            SplineMeshVertex vert;
            vert.position = sample.position + sample.normal * offset;

            // River conforms to terrain minus depth
            if (config.conform_to_terrain && terrain_height_func_) {
                float terrain_h = terrain_height_func_(vert.position.x, vert.position.z);
                vert.position.y = terrain_h - config.depth;
            }

            vert.normal = glm::vec3(0, 1, 0); // Water surface normal is up
            vert.uv = glm::vec2(u, sample.distance * config.uv_repeat);
            mesh.vertices.push_back(vert);
        }
    }

    for (int row = 0; row < num_segments; ++row) {
        for (int col = 0; col < config.width_segments; ++col) {
            uint32_t tl = row * num_cross + col;
            uint32_t tr = tl + 1;
            uint32_t bl = (row + 1) * num_cross + col;
            uint32_t br = bl + 1;

            mesh.indices.push_back(tl); mesh.indices.push_back(bl); mesh.indices.push_back(tr);
            mesh.indices.push_back(tr); mesh.indices.push_back(bl); mesh.indices.push_back(br);
        }
    }

    return mesh;
}

void SplineSystem::SetTerrainHeightFunc(TerrainHeightFunc func) {
    terrain_height_func_ = std::move(func);
}

glm::vec4 SplineSystem::CarveTerrainAlongSpline(uint32_t spline_id, float width, float depth) const {
    float length = GetSplineLength(spline_id);
    if (length <= 0.0f) return glm::vec4(0.0f);

    glm::vec3 aabb_min(std::numeric_limits<float>::max());
    glm::vec3 aabb_max(std::numeric_limits<float>::lowest());

    int samples = std::max(4, static_cast<int>(length / 2.0f));
    for (int i = 0; i <= samples; ++i) {
        float t = static_cast<float>(i) / samples;
        SplineSample s = EvaluateAtParam(spline_id, t);
        glm::vec3 left = s.position - s.normal * (width * 0.5f);
        glm::vec3 right = s.position + s.normal * (width * 0.5f);
        aabb_min = glm::min(aabb_min, glm::min(left, right));
        aabb_max = glm::max(aabb_max, glm::max(left, right));
    }

    return glm::vec4(aabb_min.x, aabb_min.z, aabb_max.x, aabb_max.z);
}

float SplineSystem::FindNearestPoint(uint32_t spline_id, const glm::vec3& world_pos) const {
    float length = GetSplineLength(spline_id);
    if (length <= 0.0f) return 0.0f;

    float best_t = 0.0f;
    float best_dist = std::numeric_limits<float>::max();

    const int search_steps = 64;
    for (int i = 0; i <= search_steps; ++i) {
        float t = static_cast<float>(i) / search_steps;
        SplineSample s = EvaluateAtParam(spline_id, t);
        float d = glm::length(s.position - world_pos);
        if (d < best_dist) { best_dist = d; best_t = t; }
    }

    // Refine with binary-style narrowing
    float step = 1.0f / search_steps;
    for (int iter = 0; iter < 8; ++iter) {
        step *= 0.5f;
        float t_left = std::max(0.0f, best_t - step);
        float t_right = std::min(1.0f, best_t + step);

        SplineSample sl = EvaluateAtParam(spline_id, t_left);
        SplineSample sr = EvaluateAtParam(spline_id, t_right);
        float dl = glm::length(sl.position - world_pos);
        float dr = glm::length(sr.position - world_pos);

        if (dl < best_dist) { best_dist = dl; best_t = t_left; }
        if (dr < best_dist) { best_dist = dr; best_t = t_right; }
    }

    return best_t;
}

void SplineSystem::RebaseOrigin(const glm::vec3& offset) {
    for (auto& s : splines_) {
        for (auto& p : s.points) {
            p.position -= offset;
        }
        s.InvalidateCache();
    }
}

void SplineSystem::Shutdown() {
    splines_.clear();
    id_to_index_.clear();
    next_id_ = 0;
    terrain_height_func_ = nullptr;
}

} // namespace terrain
} // namespace dse
