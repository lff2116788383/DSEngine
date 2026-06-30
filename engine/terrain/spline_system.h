/**
 * @file spline_system.h
 * @brief 样条系统（道路/河流）— Catmull-Rom 样条核心 + 道路/河流网格生成
 *
 * 功能：
 * - Catmull-Rom 样条：任意控制点插值，支持等距采样
 * - 道路生成：沿样条展开平面网格，支持宽度/UV/超高(banking)
 * - 河流生成：沿样条展开水面网格，附带深度/流速通道
 * - 地形贴合：自动采样地形高度，可配置嵌入深度
 * - 浮动原点：RebaseOrigin 支持
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace terrain {

/// 样条点
struct SplinePoint {
    glm::vec3 position{0.0f};
    float width = 4.0f;          ///< 该点处路面/河流宽度
    float banking = 0.0f;        ///< 超高角度（度数，正值=右侧抬升）
    glm::vec3 up{0, 1, 0};      ///< 局部上方向
};

/// 样条采样结果
struct SplineSample {
    glm::vec3 position;
    glm::vec3 tangent;           ///< 切线方向（归一化）
    glm::vec3 normal;            ///< 法线（垂直于切线的右方向）
    glm::vec3 up;                ///< 上方向
    float width;                 ///< 插值宽度
    float banking;               ///< 插值超高
    float distance;              ///< 距离起点的弧长
};

/// 道路/河流网格顶点
struct SplineMeshVertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 uv;                ///< u = 横向[0,1]，v = 沿样条方向
};

/// 道路/河流网格
struct SplineMesh {
    std::vector<SplineMeshVertex> vertices;
    std::vector<uint32_t> indices;
};

/// 道路配置
struct RoadConfig {
    float segment_length = 1.0f;       ///< 沿样条方向的分段长度
    int width_segments = 4;            ///< 横向细分数
    float uv_repeat = 0.1f;            ///< V方向UV重复率（每米）
    float embed_depth = 0.1f;          ///< 嵌入地形的深度
    bool conform_to_terrain = true;    ///< 是否贴合地形高度
};

/// 河流配置
struct RiverConfig {
    float segment_length = 2.0f;
    int width_segments = 6;
    float uv_repeat = 0.05f;
    float depth = 2.0f;                ///< 河流深度
    float flow_speed = 1.0f;           ///< 流速（影响UV动画）
    bool conform_to_terrain = true;
};

/// 高度采样回调
using TerrainHeightFunc = std::function<float(float x, float z)>;

/// 样条系统
class DSE_EXPORT SplineSystem {
public:
    SplineSystem() = default;
    ~SplineSystem() = default;

    // === 样条管理 ===

    /// 创建样条，返回 spline_id
    uint32_t CreateSpline(const std::string& name);

    /// 删除样条
    void DestroySpline(uint32_t spline_id);

    /// 获取样条数量
    uint32_t GetSplineCount() const { return static_cast<uint32_t>(splines_.size()); }

    // === 控制点操作 ===

    /// 添加控制点到样条末尾
    void AddPoint(uint32_t spline_id, const SplinePoint& point);

    /// 在指定索引插入控制点
    void InsertPoint(uint32_t spline_id, uint32_t index, const SplinePoint& point);

    /// 删除控制点
    void RemovePoint(uint32_t spline_id, uint32_t index);

    /// 设置控制点
    void SetPoint(uint32_t spline_id, uint32_t index, const SplinePoint& point);

    /// 获取控制点数量
    uint32_t GetPointCount(uint32_t spline_id) const;

    /// 获取控制点
    SplinePoint GetPoint(uint32_t spline_id, uint32_t index) const;

    // === 样条评估 ===

    /// 按参数 t∈[0,1] 在样条上采样
    SplineSample EvaluateAtParam(uint32_t spline_id, float t) const;

    /// 按弧长距离在样条上采样
    SplineSample EvaluateAtDistance(uint32_t spline_id, float distance) const;

    /// 获取样条总长度
    float GetSplineLength(uint32_t spline_id) const;

    /// 等距采样：返回 count 个均匀分布的采样点
    std::vector<SplineSample> SampleUniform(uint32_t spline_id, uint32_t count) const;

    // === 网格生成 ===

    /// 生成道路网格
    SplineMesh GenerateRoadMesh(uint32_t spline_id, const RoadConfig& config) const;

    /// 生成河流网格
    SplineMesh GenerateRiverMesh(uint32_t spline_id, const RiverConfig& config) const;

    // === 地形集成 ===

    /// 设置地形高度采样回调
    void SetTerrainHeightFunc(TerrainHeightFunc func);

    /// 沿样条刻蚀地形（返回受影响的AABB）
    glm::vec4 CarveTerrainAlongSpline(uint32_t spline_id, float width, float depth) const;

    // === 大世界支持 ===

    /// 浮动原点重定位
    void RebaseOrigin(const glm::vec3& offset);

    /// 查找最近样条点
    float FindNearestPoint(uint32_t spline_id, const glm::vec3& world_pos) const;

    void Shutdown();

private:
    struct SplineData {
        std::string name;
        std::vector<SplinePoint> points;
        mutable float cached_length = -1.0f;
        mutable std::vector<float> segment_lengths;

        void InvalidateCache() const { cached_length = -1.0f; segment_lengths.clear(); }
    };

    void EnsureLengthCache(const SplineData& spline) const;
    glm::vec3 CatmullRomInterp(const glm::vec3& p0, const glm::vec3& p1,
                                const glm::vec3& p2, const glm::vec3& p3, float t) const;
    float LerpFloat(float a, float b, float t) const { return a + (b - a) * t; }

    std::vector<SplineData> splines_;
    TerrainHeightFunc terrain_height_func_;
    uint32_t next_id_ = 0;
    std::vector<uint32_t> id_to_index_;  // maps spline_id → index in splines_
};

} // namespace terrain
} // namespace dse
