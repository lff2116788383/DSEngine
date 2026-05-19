/**
 * @file cluster_grid.h
 * @brief Clustered Forward+ 视锥体 3D 网格 - CPU 端光源分簇
 *
 * 网格参数：
 *   - XY: 每 tile 16×16 像素
 *   - Z:  24 个对数分片 (near → far)
 *
 * 输出 SSBO：
 *   binding 2: ClusterInfo[] — 每个 cluster 的光源列表偏移和数量
 *   binding 3: uint[]        — 扁平化光源索引数组（先 point 后 spot）
 */

#ifndef DSE_RENDER_CLUSTER_GRID_H
#define DSE_RENDER_CLUSTER_GRID_H

#include <vector>
#include <cstdint>
#include <glm/glm.hpp>
#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class RhiDevice;

struct GPUPointLight;
struct GPUSpotLight;

/// SSBO 绑定点 — 避开 PointLightSSBO(1) 和 SpotLightSSBO(2)
static constexpr unsigned int kSSBOBindingClusterInfo   = 3;
static constexpr unsigned int kSSBOBindingLightIndices  = 4;

/// 网格常量
static constexpr int kClusterTileSize = 16;
static constexpr int kClusterZSlices  = 24;

/// SSBO 头部 — 嵌入 cluster 网格参数，shader 直接读取（32 bytes）
struct ClusterGridHeader {
    uint32_t tiles_x;
    uint32_t tiles_y;
    uint32_t z_slices;
    float    near_plane;
    float    far_plane;
    uint32_t _pad0, _pad1, _pad2;
};
static_assert(sizeof(ClusterGridHeader) == 32, "ClusterGridHeader must be 32 bytes");

/// 每 cluster 元数据（16 bytes，与 SSBO std430 对齐）
struct ClusterInfo {
    uint32_t offset;       ///< light_indices 数组中的起始偏移
    uint32_t point_count;  ///< 该 cluster 中的点光源数量
    uint32_t spot_count;   ///< 该 cluster 中的聚光灯数量
    uint32_t _pad;
};
static_assert(sizeof(ClusterInfo) == 16, "ClusterInfo must be 16 bytes");

// ============================================================
// ClusterGrid
// ============================================================

class ClusterGrid {
public:
    ClusterGrid() = default;
    ~ClusterGrid() = default;

    /// 初始化 SSBO（RHI 设备就绪后调用）
    void Init(RhiDevice* device);

    /// CPU 端构建 cluster 光源分配
    void Build(const glm::mat4& view, const glm::mat4& projection,
               float near_plane, float far_plane,
               int screen_width, int screen_height,
               const std::vector<GPUPointLight>& point_lights,
               const std::vector<GPUSpotLight>& spot_lights);

    /// 上传到 SSBO
    void Upload();

    /// 绑定 SSBO
    void Bind();

    /// 释放资源
    void Shutdown();

    // --- 只读访问 ---
    int tiles_x() const { return tiles_x_; }
    int tiles_y() const { return tiles_y_; }
    int total_clusters() const { return tiles_x_ * tiles_y_ * kClusterZSlices; }

private:
    /// 计算指定 cluster 在 view space 中的 AABB
    void ComputeClusterAABB(int tx, int ty, int tz,
                            float near_plane, float far_plane,
                            const glm::mat4& inv_proj,
                            glm::vec3& out_min, glm::vec3& out_max) const;

    /// 球体-AABB 相交测试
    static bool SphereAABBIntersect(const glm::vec3& center, float radius,
                                    const glm::vec3& aabb_min, const glm::vec3& aabb_max);

    RhiDevice* device_ = nullptr;
    int tiles_x_ = 0;
    int tiles_y_ = 0;
    int screen_width_ = 0;
    int screen_height_ = 0;

    // CPU 端数据
    ClusterGridHeader header_{};
    std::vector<ClusterInfo> cluster_infos_;
    std::vector<uint32_t> light_indices_;

    // GPU SSBO 句柄
    BufferHandle cluster_info_ssbo_;
    BufferHandle light_index_ssbo_;

    // SSBO 容量跟踪
    size_t cluster_info_capacity_bytes_ = 0;
    size_t light_index_capacity_bytes_  = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_CLUSTER_GRID_H
