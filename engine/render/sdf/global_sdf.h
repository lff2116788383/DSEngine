/**
 * @file global_sdf.h
 * @brief 全局有符号距离场（Global SDF）系统
 *
 * 在观察者周围维护一个分级体素化的有符号距离场：
 * - 多级级联（cascade）覆盖不同距离范围（类似 CSM 设计）
 * - 每级为 3D 纹理（R16F），存储到最近表面的有符号距离
 * - 用途：大规模软阴影 cone trace、AO、GI 遮挡、植被碰撞
 * - 每帧增量更新：仅重新光栅化已移动的体素切片
 */

#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace render {

/// 单个 SDF Cascade
struct SDFCascade {
    int resolution = 0;         ///< 3D 纹理边长（体素数，如 64/128）
    float voxel_size = 0.0f;    ///< 单体素世界尺寸
    float extent = 0.0f;        ///< 本级覆盖半径
    glm::vec3 center{0.0f};    ///< 当前中心（跟随观察者 snap）

    /// CPU 侧距离数据（resolution^3）用于初始化和调试
    std::vector<float> distance_data;
    bool gpu_dirty = true;
};

/// Global SDF 配置
struct GlobalSDFConfig {
    int num_cascades = 4;           ///< 级联数量（通常 3-5）
    int base_resolution = 64;       ///< 第 0 级分辨率
    float base_voxel_size = 0.5f;   ///< 第 0 级体素尺寸（世界单位）
    float cascade_ratio = 2.0f;     ///< 级间尺寸倍数
    int max_updates_per_frame = 4;  ///< 每帧最多更新的切片数
};

/// 三角形网格几何体（用于体素化输入）
struct SDFMeshInput {
    const glm::vec3* positions = nullptr;
    const uint32_t* indices = nullptr;
    uint32_t vertex_count = 0;
    uint32_t index_count = 0;
    glm::mat4 transform{1.0f};
};

/// Global SDF 系统
class DSE_EXPORT GlobalSDFSystem {
public:
    GlobalSDFSystem() = default;
    ~GlobalSDFSystem() = default;

    /// 初始化 SDF 级联
    void Init(const GlobalSDFConfig& config);

    /// 每帧更新：根据观察者位置滚动级联中心
    void Update(const glm::vec3& viewer_pos);

    /// 提交静态几何体用于 SDF 体素化
    void SubmitStaticMesh(const SDFMeshInput& mesh);

    /// 触发全量体素化（初始化或场景切换后调用）
    void RebuildAll();

    /// 查询世界空间某点的 SDF 值（返回到最近表面距离，负数表示在内部）
    float QueryDistance(const glm::vec3& world_pos) const;

    /// 获取某级数据
    const SDFCascade& GetCascade(int index) const { return cascades_[index]; }

    /// 获取级联数量
    int CascadeCount() const { return static_cast<int>(cascades_.size()); }

    /// 获取需要 GPU 更新的级联
    std::vector<int> GetDirtyCascades() const;

    /// 标记已上传
    void ClearDirty(int cascade);

    void Shutdown();

private:
    /// 对单个体素计算到 mesh soup 的最近距离
    float ComputeDistanceAtVoxel(const glm::vec3& voxel_center) const;

    /// 点到三角形最短距离
    static float PointTriangleDistance(const glm::vec3& p,
                                        const glm::vec3& a, const glm::vec3& b, const glm::vec3& c);

    GlobalSDFConfig config_;
    std::vector<SDFCascade> cascades_;
    std::vector<SDFMeshInput> static_meshes_;
    bool initialized_ = false;
};

} // namespace render
} // namespace dse
