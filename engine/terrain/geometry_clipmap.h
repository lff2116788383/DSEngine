/**
 * @file geometry_clipmap.h
 * @brief Geometry Clipmap 连续 LOD 地形系统
 *
 * 基于 Losasso & Hoppe 2004 "Geometry Clipmaps" 论文思想：
 * - 以观察者为中心构建多层同心环（ring），每层网格分辨率翻倍
 * - 相邻层间使用 transition blend 消除接缝
 * - 每帧只需更新移动方向上的一行/列（toroidal addressing）
 * - 距离远处自动降低顶点密度，无需预烘焙 LOD mesh
 *
 * 与 TerrainTileManager 互补：TileManager 管理数据加载/卸载，
 * GeometryClipmap 负责连续 LOD 几何生成与渲染。
 */

#pragma once

#include <cstdint>
#include <vector>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace terrain {

/// Clipmap 单层配置
struct ClipmapLevel {
    int grid_size = 0;          ///< 本层网格边长（顶点数，含边界）
    float cell_size = 0.0f;     ///< 本层单格世界尺寸（= base_cell_size * 2^level）
    float extent = 0.0f;        ///< 本层覆盖范围半径（world units）

    /// Toroidal origin：当前 heightmap 数据在环形缓冲中的逻辑原点
    int origin_x = 0;
    int origin_z = 0;

    /// 顶点/索引数据（CPU 侧，需要时上传 GPU）
    std::vector<float> height_data;  ///< grid_size * grid_size 的高度值
    bool gpu_dirty = true;           ///< 需要重新上传 GPU
    bool needs_fill = true;          ///< 首次需要完整填充
};

/// Geometry Clipmap 配置
struct GeometryClipmapConfig {
    int num_levels = 6;             ///< clipmap 层数（通常 4-8）
    int grid_size = 64;             ///< 每层网格边长（顶点数，2^n，通常 64 或 128）
    float base_cell_size = 1.0f;    ///< 第 0 层（最精细）的格子世界尺寸
    float height_scale = 100.0f;    ///< 高度缩放因子
    float blend_width = 4.0f;       ///< 层间过渡混合宽度（格子数）
};

/// 高度数据采样回调：给定世界坐标 (x,z) 返回高度值 [0,1]
using HeightSampleFunc = float(*)(float world_x, float world_z, void* user_data);

/// Geometry Clipmap 系统
class DSE_EXPORT GeometryClipmapSystem {
public:
    GeometryClipmapSystem() = default;
    ~GeometryClipmapSystem() = default;

    /// 初始化 clipmap：分配层级缓冲区
    void Init(const GeometryClipmapConfig& config);

    /// 设置高度采样回调
    void SetHeightSampler(HeightSampleFunc func, void* user_data = nullptr);

    /// 每帧更新：根据观察者位置更新 toroidal 数据
    /// @param viewer_pos 观察者世界坐标（通常是相机位置）
    void Update(const glm::vec3& viewer_pos);

    /// 获取某一层级数据（用于渲染）
    const ClipmapLevel& GetLevel(int level) const { return levels_[level]; }

    /// 获取层级数量
    int LevelCount() const { return static_cast<int>(levels_.size()); }

    /// 获取配置
    const GeometryClipmapConfig& GetConfig() const { return config_; }

    /// 获取指定世界坐标的 clipmap 高度（插值查询）
    float SampleHeight(float world_x, float world_z) const;

    /// 获取需要更新 GPU 的层级列表
    std::vector<int> GetDirtyLevels() const;

    /// 标记某层已上传 GPU
    void ClearDirty(int level);

    void Shutdown();

private:
    /// 更新单层的 toroidal addressing
    void UpdateLevel(int level, const glm::vec3& viewer_pos);

    /// 采样指定世界坐标的高度
    float FetchHeight(float world_x, float world_z) const;

    GeometryClipmapConfig config_;
    std::vector<ClipmapLevel> levels_;
    HeightSampleFunc height_sampler_ = nullptr;
    void* sampler_user_data_ = nullptr;
    glm::vec3 last_viewer_pos_{0.0f};
    bool initialized_ = false;
};

} // namespace terrain
} // namespace dse
