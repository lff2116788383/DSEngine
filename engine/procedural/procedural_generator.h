/**
 * @file procedural_generator.h
 * @brief 运行时程序化生成系统
 *
 * 提供噪声函数库 + 规则驱动的地形/植被/道具散布生成器：
 * - Perlin / Simplex / Worley / FBM 噪声函数
 * - 地形高度图生成（多层 FBM + domain warping + erosion mask）
 * - 散布规则（Poisson disk + density map + slope filter + height filter）
 * - 确定性种子保证相同输入相同输出（multiplayer 一致性）
 * - 按 chunk/cell 粒度按需生成，与 WorldPartition 集成
 */

#pragma once

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace procedural {

// ─── 噪声函数 ──────────────────────────────────────────────────────────────

/// 2D Perlin noise [-1, 1]
DSE_EXPORT float PerlinNoise2D(float x, float y, uint32_t seed = 0);

/// 2D Simplex noise [-1, 1]
DSE_EXPORT float SimplexNoise2D(float x, float y, uint32_t seed = 0);

/// 2D Worley/Cellular noise [0, 1]（返回到最近特征点的距离）
DSE_EXPORT float WorleyNoise2D(float x, float y, uint32_t seed = 0);

/// Fractal Brownian Motion（多层叠加）
struct FBMParams {
    int octaves = 6;
    float frequency = 1.0f;
    float lacunarity = 2.0f;        ///< 频率倍增因子
    float persistence = 0.5f;       ///< 振幅衰减因子
    uint32_t seed = 0;
};

DSE_EXPORT float FBM2D(float x, float y, const FBMParams& params);

/// Domain Warping（坐标扭曲，产生更自然的地形形态）
DSE_EXPORT float DomainWarpedFBM(float x, float y, const FBMParams& params, float warp_strength = 0.5f);

// ─── 地形生成 ──────────────────────────────────────────────────────────────

/// 地形生成配置
struct TerrainGenConfig {
    uint32_t seed = 12345;
    float base_frequency = 0.002f;      ///< 基础频率（越小地形越平缓）
    int octaves = 6;
    float height_scale = 200.0f;        ///< 最大高度
    float lacunarity = 2.1f;
    float persistence = 0.45f;
    float warp_strength = 0.4f;         ///< Domain warping 强度
    float plateau_threshold = 0.7f;     ///< 高原削顶阈值
    float valley_power = 1.5f;          ///< 山谷加深指数
};

/// 生成指定区域的高度图
/// @param out_heights 输出高度数组（row-major, size = width * height）
/// @param world_origin_x 区域左下角世界 X
/// @param world_origin_z 区域左下角世界 Z
/// @param width 采样点宽度
/// @param height 采样点高度
/// @param sample_spacing 采样间距（世界单位）
DSE_EXPORT void GenerateHeightmap(std::vector<float>& out_heights,
                                   float world_origin_x, float world_origin_z,
                                   int width, int height, float sample_spacing,
                                   const TerrainGenConfig& config);

// ─── 散布/实例化 ────────────────────────────────────────────────────────────

/// 散布物类型
struct ScatterType {
    std::string asset_path;             ///< 资产路径（mesh/prefab）
    float min_scale = 0.8f;
    float max_scale = 1.2f;
    float min_height = -1000.0f;        ///< 高度范围过滤
    float max_height = 1000.0f;
    float max_slope = 45.0f;            ///< 最大坡度（度）
    float density = 1.0f;               ///< 密度因子（0-1，相对于 base_spacing）
};

/// 散布实例结果
struct ScatterInstance {
    glm::vec3 position{0.0f};
    glm::vec3 rotation{0.0f};           ///< Euler angles (degrees)
    float scale = 1.0f;
    uint32_t type_index = 0;            ///< ScatterType 数组中的索引
};

/// 散布生成配置
struct ScatterConfig {
    uint32_t seed = 42;
    float base_spacing = 5.0f;          ///< Poisson disk 基础间距
    std::vector<ScatterType> types;
    bool align_to_normal = true;        ///< 是否对齐地面法线
    float random_rotation_y = 360.0f;   ///< Y 轴随机旋转范围（度）
};

/// 高度查询回调
using HeightQueryFunc = std::function<float(float x, float z)>;
/// 法线查询回调
using NormalQueryFunc = std::function<glm::vec3(float x, float z)>;

/// 生成指定区域的散布实例
/// @param world_origin_x/z 区域左下角
/// @param extent_x/z 区域尺寸
DSE_EXPORT void GenerateScatter(std::vector<ScatterInstance>& out_instances,
                                 float world_origin_x, float world_origin_z,
                                 float extent_x, float extent_z,
                                 const ScatterConfig& config,
                                 HeightQueryFunc height_func,
                                 NormalQueryFunc normal_func = nullptr);

// ─── PCG 随机工具 ──────────────────────────────────────────────────────────

/// PCG 伪随机数生成器（确定性，可重复）
class DSE_EXPORT PCGRandom {
public:
    explicit PCGRandom(uint64_t seed = 0);

    /// [0, 2^32)
    uint32_t Next();
    /// [0.0, 1.0)
    float NextFloat();
    /// [min, max)
    float Range(float min_val, float max_val);

private:
    uint64_t state_;
    uint64_t inc_;
};

} // namespace procedural
} // namespace dse
