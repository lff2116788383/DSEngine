/**
 * @file mesh_decimator.h
 * @brief QEM (Quadric Error Metrics) 网格减面器
 *
 * 基于 Garland & Heckbert 1997 的 Surface Simplification Using Quadric Error Metrics。
 * 迭代折叠代价最小的边，直至达到目标三角形数或误差阈值。
 *
 * 特性：
 * - 顶点属性感知（法线/UV/颜色权重进入误差计算）
 * - UV 接缝 & 材质边界保护（可选边界锁定）
 * - 拓扑安全（翻转检测、非流形检测）
 * - 支持逐 submesh 独立减面或全 mesh 统一减面
 *
 * 使用方式：
 *   MeshDecimator dec;
 *   DecimationResult result = dec.Decimate(input, config);
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <glm/glm.hpp>

namespace dse {
namespace mesh {

/// 减面配置
struct DecimationConfig {
    /// 目标三角形比例 [0,1]；0.5 = 保留 50% 三角形
    float target_ratio = 0.5f;

    /// 目标三角形绝对数量（0 = 使用 target_ratio）
    uint32_t target_triangle_count = 0;

    /// 最大误差阈值：超过此值的折叠操作不执行（0 = 无限制）
    float max_error = 0.0f;

    /// UV 接缝权重乘子（>1 更保护接缝）
    float seam_weight = 2.0f;

    /// 边界边权重乘子（>1 更保护边界）
    float boundary_weight = 10.0f;

    /// 法线翻转检测阈值（dot < threshold 判定为翻转，阻止折叠）
    float normal_flip_threshold = 0.2f;

    /// 是否锁定边界顶点（完全不动）
    bool lock_boundary = false;

    /// 是否保护 UV 接缝上的顶点（降低折叠优先级但不完全锁定）
    bool protect_uv_seams = true;

    /// 属性权重（法线/UV 差异对误差的贡献）
    float attribute_weight = 1.0f;
};

/// 输入 submesh 数据（引用外部缓冲，不拷贝）
struct DecimationInput {
    const glm::vec3* positions = nullptr;
    const glm::vec3* normals = nullptr;    ///< 可选
    const glm::vec2* texcoords = nullptr;  ///< 可选
    const glm::vec4* colors = nullptr;     ///< 可选
    uint32_t vertex_count = 0;

    const uint32_t* indices = nullptr;
    uint32_t index_count = 0;
};

/// 减面结果
struct DecimationResult {
    std::vector<glm::vec3> positions;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<glm::vec4> colors;
    std::vector<uint32_t> indices;

    uint32_t original_triangle_count = 0;
    uint32_t result_triangle_count = 0;
    float max_error_incurred = 0.0f;
    bool success = false;
};

/// LOD 生成配置（一次生成多级）
struct LodGenerationConfig {
    /// 每级的比例，如 {0.5, 0.25, 0.125} 表示 LOD1=50%, LOD2=25%, LOD3=12.5%
    std::vector<float> level_ratios;

    /// 各级通用的减面参数（seam_weight, boundary_weight 等）
    DecimationConfig base_config;
};

/// LOD 生成结果
struct LodGenerationResult {
    std::vector<DecimationResult> levels;  ///< 与 level_ratios 一一对应
    bool success = false;
};

/**
 * @class MeshDecimator
 * @brief QEM 网格减面器
 */
class MeshDecimator {
public:
    MeshDecimator() = default;
    ~MeshDecimator() = default;

    /// 对单个 mesh 执行减面
    DecimationResult Decimate(const DecimationInput& input, const DecimationConfig& config);

    /// 生成多级 LOD mesh
    LodGenerationResult GenerateLods(const DecimationInput& input, const LodGenerationConfig& config);
};

} // namespace mesh
} // namespace dse
