/**
 * @file cluster_grid.cpp
 * @brief ClusterGrid 实现 - CPU 端光源分簇 + SSBO 上传
 */

#include "engine/render/cluster_grid.h"
#include "engine/render/light_buffer.h"
#include "engine/render/rhi/rhi_device.h"
#include <algorithm>
#include <cmath>

namespace dse {
namespace render {

void ClusterGrid::Init(RhiDevice* device) {
    device_ = device;
}

void ClusterGrid::Build(const glm::mat4& view, const glm::mat4& projection,
                         float near_plane, float far_plane,
                         int screen_width, int screen_height,
                         const std::vector<GPUPointLight>& point_lights,
                         const std::vector<GPUSpotLight>& spot_lights) {
    screen_width_  = screen_width;
    screen_height_ = screen_height;
    tiles_x_ = (screen_width  + kClusterTileSize - 1) / kClusterTileSize;
    tiles_y_ = (screen_height + kClusterTileSize - 1) / kClusterTileSize;

    // 避免退化情况
    if (near_plane <= 0.0f) near_plane = 0.1f;
    if (far_plane <= near_plane) far_plane = near_plane + 100.0f;

    const int total = tiles_x_ * tiles_y_ * kClusterZSlices;
    cluster_infos_.resize(total);
    light_indices_.clear();

    // ---- Cluster view-space AABB 缓存 ----
    // AABB 只依赖 (projection, near, far, screen_w, screen_h)；相机移动/光源变化
    // 不影响 AABB（它们在 view space 且由投影+屏幕尺寸唯一决定）。仅在这些参数变化
    // 时重算，避免每帧 total×8 次逆投影射线计算。
    const bool aabb_dirty =
        !aabb_cache_valid_ ||
        static_cast<int>(cluster_aabb_min_.size()) != total ||
        cached_near_ != near_plane ||
        cached_far_ != far_plane ||
        cached_screen_width_ != screen_width_ ||
        cached_screen_height_ != screen_height_ ||
        cached_projection_ != projection;

    if (aabb_dirty) {
        const glm::mat4 inv_proj = glm::inverse(projection);
        cluster_aabb_min_.resize(total);
        cluster_aabb_max_.resize(total);
        for (int tz = 0; tz < kClusterZSlices; ++tz) {
            for (int ty = 0; ty < tiles_y_; ++ty) {
                for (int tx = 0; tx < tiles_x_; ++tx) {
                    const int idx = (tz * tiles_y_ + ty) * tiles_x_ + tx;
                    ComputeClusterAABB(tx, ty, tz, near_plane, far_plane, inv_proj,
                                       cluster_aabb_min_[idx], cluster_aabb_max_[idx]);
                }
            }
        }
        cached_projection_ = projection;
        cached_near_ = near_plane;
        cached_far_ = far_plane;
        cached_screen_width_ = screen_width_;
        cached_screen_height_ = screen_height_;
        aabb_cache_valid_ = true;
    }

    // 将光源位置变换到 view space
    struct ViewSpaceLight {
        glm::vec3 pos;
        float radius;
    };

    std::vector<ViewSpaceLight> vs_point_lights(point_lights.size());
    for (size_t i = 0; i < point_lights.size(); ++i) {
        glm::vec4 vp = view * glm::vec4(point_lights[i].position, 1.0f);
        vs_point_lights[i].pos = glm::vec3(vp);
        vs_point_lights[i].radius = point_lights[i].radius;
    }

    std::vector<ViewSpaceLight> vs_spot_lights(spot_lights.size());
    for (size_t i = 0; i < spot_lights.size(); ++i) {
        glm::vec4 vp = view * glm::vec4(spot_lights[i].position, 1.0f);
        vs_spot_lights[i].pos = glm::vec3(vp);
        vs_spot_lights[i].radius = spot_lights[i].radius;
    }

    // 临时 per-cluster 光源索引列表
    struct ClusterLights {
        std::vector<uint32_t> point_indices;
        std::vector<uint32_t> spot_indices;
    };
    std::vector<ClusterLights> temp_clusters(total);

    // 遍历每个 cluster，测试光源相交（AABB 取自缓存）
    for (int idx = 0; idx < total; ++idx) {
        const glm::vec3& aabb_min = cluster_aabb_min_[idx];
        const glm::vec3& aabb_max = cluster_aabb_max_[idx];

        // 测试点光源
        for (size_t li = 0; li < vs_point_lights.size(); ++li) {
            if (SphereAABBIntersect(vs_point_lights[li].pos, vs_point_lights[li].radius,
                                    aabb_min, aabb_max)) {
                temp_clusters[idx].point_indices.push_back(static_cast<uint32_t>(li));
            }
        }

        // 测试聚光灯（用包围球近似）
        for (size_t li = 0; li < vs_spot_lights.size(); ++li) {
            if (SphereAABBIntersect(vs_spot_lights[li].pos, vs_spot_lights[li].radius,
                                    aabb_min, aabb_max)) {
                temp_clusters[idx].spot_indices.push_back(static_cast<uint32_t>(li));
            }
        }
    }

    // 压缩为扁平数组
    light_indices_.clear();
    for (int i = 0; i < total; ++i) {
        cluster_infos_[i].offset = static_cast<uint32_t>(light_indices_.size());
        cluster_infos_[i].point_count = static_cast<uint32_t>(temp_clusters[i].point_indices.size());
        cluster_infos_[i].spot_count  = static_cast<uint32_t>(temp_clusters[i].spot_indices.size());
        cluster_infos_[i]._pad = 0;

        // 先存点光源索引，再存聚光灯索引
        for (uint32_t pi : temp_clusters[i].point_indices) {
            light_indices_.push_back(pi);
        }
        for (uint32_t si : temp_clusters[i].spot_indices) {
            light_indices_.push_back(si);
        }
    }

    // 填充 SSBO 头部
    header_.tiles_x    = static_cast<uint32_t>(tiles_x_);
    header_.tiles_y    = static_cast<uint32_t>(tiles_y_);
    header_.z_slices   = static_cast<uint32_t>(kClusterZSlices);
    header_.near_plane = near_plane;
    header_.far_plane  = far_plane;
    header_._pad0 = header_._pad1 = header_._pad2 = 0;
}

void ClusterGrid::Upload() {
    if (!device_) return;

    const size_t header_bytes = sizeof(ClusterGridHeader);
    const size_t info_bytes = cluster_infos_.size() * sizeof(ClusterInfo);
    const size_t total_info_bytes = header_bytes + info_bytes;
    const size_t index_bytes = light_indices_.empty() ? sizeof(uint32_t) : light_indices_.size() * sizeof(uint32_t);

    // ClusterInfo SSBO = header + ClusterInfo[]。per-in-flight ring：写当前槽位（其 fence
    // 已在 BeginFrame/AcquireNextImage 等待），仅(重)建当前槽位、不触发跨帧总闸。
    cluster_info_ssbo_ = cluster_info_ring_.Acquire(*device_, total_info_bytes, GpuBufferUsage::kStorage);
    if (cluster_info_ssbo_) {
        device_->UpdateGpuBuffer(cluster_info_ssbo_, 0, header_bytes, &header_);
        if (!cluster_infos_.empty()) {
            device_->UpdateGpuBuffer(cluster_info_ssbo_, header_bytes, info_bytes, cluster_infos_.data());
        }
    }

    // Light index SSBO
    light_index_ssbo_ = light_index_ring_.Acquire(*device_, index_bytes, GpuBufferUsage::kStorage);
    if (light_index_ssbo_ && !light_indices_.empty()) {
        device_->UpdateGpuBuffer(light_index_ssbo_, 0, light_indices_.size() * sizeof(uint32_t), light_indices_.data());
    }
}

void ClusterGrid::Bind() {
    if (!device_) return;
    device_->BindGpuBuffer(cluster_info_ssbo_,  kSSBOBindingClusterInfo);
    device_->BindGpuBuffer(light_index_ssbo_,   kSSBOBindingLightIndices);
}

void ClusterGrid::Shutdown() {
    if (!device_) return;
    cluster_info_ring_.Shutdown(*device_);
    light_index_ring_.Shutdown(*device_);
    cluster_info_ssbo_ = {};
    light_index_ssbo_ = {};
    cluster_infos_.clear();
    light_indices_.clear();
    cluster_aabb_min_.clear();
    cluster_aabb_max_.clear();
    aabb_cache_valid_ = false;
    device_ = nullptr;
}

// ============================================================
// Cluster AABB 计算（view space）
// ============================================================

void ClusterGrid::ComputeClusterAABB(int tx, int ty, int tz,
                                      float near_plane, float far_plane,
                                      const glm::mat4& inv_proj,
                                      glm::vec3& out_min, glm::vec3& out_max) const {
    // 对数 Z 分片
    const float log_ratio = std::log(far_plane / near_plane);
    const float z_near = near_plane * std::exp(log_ratio * static_cast<float>(tz) / static_cast<float>(kClusterZSlices));
    const float z_far  = near_plane * std::exp(log_ratio * static_cast<float>(tz + 1) / static_cast<float>(kClusterZSlices));

    // Tile 屏幕坐标 → NDC [-1, 1]
    const float x0_ndc = static_cast<float>(tx * kClusterTileSize) / static_cast<float>(screen_width_) * 2.0f - 1.0f;
    const float x1_ndc = static_cast<float>((tx + 1) * kClusterTileSize) / static_cast<float>(screen_width_) * 2.0f - 1.0f;
    const float y0_ndc = static_cast<float>(ty * kClusterTileSize) / static_cast<float>(screen_height_) * 2.0f - 1.0f;
    const float y1_ndc = static_cast<float>((ty + 1) * kClusterTileSize) / static_cast<float>(screen_height_) * 2.0f - 1.0f;

    // 在 view space 中，将 NDC XY 范围通过逆投影射线变换到对应 Z 深度的 XY 范围
    // 射线方向 = inv_proj * ndc_corner (在 z=-1, w=1 的裁剪空间)
    auto UnprojectCorner = [&](float ndc_x, float ndc_y, float view_z) -> glm::vec3 {
        // NDC → clip space (z=-1 for near plane in OpenGL convention)
        glm::vec4 clip(ndc_x, ndc_y, -1.0f, 1.0f);
        glm::vec4 view_pos = inv_proj * clip;
        view_pos /= view_pos.w;
        // 射线从原点出发，缩放到目标 Z 深度（view space Z 为负方向）
        float t = view_z / view_pos.z;
        return glm::vec3(view_pos.x * t, view_pos.y * t, view_z);
    };

    // view space 中 Z 轴指向 -Z（OpenGL 惯例），所以近平面 z = -z_near
    const float vz_near = -z_near;
    const float vz_far  = -z_far;

    // 计算 4 个角在近/远平面的位置，取 AABB
    glm::vec3 corners[8] = {
        UnprojectCorner(x0_ndc, y0_ndc, vz_near),
        UnprojectCorner(x1_ndc, y0_ndc, vz_near),
        UnprojectCorner(x0_ndc, y1_ndc, vz_near),
        UnprojectCorner(x1_ndc, y1_ndc, vz_near),
        UnprojectCorner(x0_ndc, y0_ndc, vz_far),
        UnprojectCorner(x1_ndc, y0_ndc, vz_far),
        UnprojectCorner(x0_ndc, y1_ndc, vz_far),
        UnprojectCorner(x1_ndc, y1_ndc, vz_far),
    };

    out_min = corners[0];
    out_max = corners[0];
    for (int i = 1; i < 8; ++i) {
        out_min = glm::min(out_min, corners[i]);
        out_max = glm::max(out_max, corners[i]);
    }
}

// ============================================================
// 球体-AABB 相交
// ============================================================

bool ClusterGrid::SphereAABBIntersect(const glm::vec3& center, float radius,
                                       const glm::vec3& aabb_min, const glm::vec3& aabb_max) {
    // 找到 AABB 上距离球心最近的点
    glm::vec3 closest = glm::clamp(center, aabb_min, aabb_max);
    glm::vec3 diff = closest - center;
    float dist_sq = glm::dot(diff, diff);
    return dist_sq <= (radius * radius);
}

} // namespace render
} // namespace dse
