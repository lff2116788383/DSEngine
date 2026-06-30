/**
 * @file ocean_system.h
 * @brief 大规模海洋系统 — Tile-based FFT Ocean + LOD + 泡沫/焦散
 *
 * 功能：
 * - FFT 海洋波谱模拟（Phillips/JONSWAP）
 * - Tile-based LOD：近处高频细节，远处低频概形
 * - 泡沫（Foam）：基于雅可比行列式的白浪生成
 * - 焦散（Caustics）：基于法线折射的焦散贴图生成
 * - 浅水着色：基于深度的颜色渐变 + 散射
 * - 浮动原点支持
 */

#pragma once

#include <cstdint>
#include <vector>
#include <complex>
#include <glm/glm.hpp>
#include "engine/core/dse_export.h"

namespace dse {
namespace render {

/// 海洋波谱类型
enum class OceanSpectrumType : uint8_t {
    Phillips = 0,     ///< Phillips 谱（经典）
    JONSWAP = 1,      ///< JONSWAP 谱（更物理）
    PiersonMoskowitz = 2
};

/// 海洋 LOD 层级
struct OceanLODLevel {
    int grid_size = 0;         ///< 该层网格分辨率
    float tile_size = 0.0f;    ///< 该层 tile 世界尺寸
    float max_distance = 0.0f; ///< 该层最大可见距离
};

/// 海洋配置
struct OceanConfig {
    int fft_resolution = 256;         ///< FFT 分辨率（2^n）
    float tile_size = 100.0f;         ///< 单 tile 世界尺寸（米）
    int tile_count = 8;               ///< 可见 tile 数（每方向，总计 tile_count²）
    int lod_levels = 4;               ///< LOD 层数

    // 波浪参数
    OceanSpectrumType spectrum = OceanSpectrumType::Phillips;
    float wind_speed = 20.0f;         ///< 风速 (m/s)
    glm::vec2 wind_direction{1, 0};   ///< 风向（归一化）
    float amplitude = 0.0004f;        ///< 波幅因子
    float gravity = 9.81f;
    float choppiness = 1.2f;          ///< 横向位移系数

    // 渲染
    glm::vec3 shallow_color{0.0f, 0.5f, 0.5f};
    glm::vec3 deep_color{0.0f, 0.05f, 0.15f};
    float foam_threshold = -0.2f;     ///< 雅可比 < 此值产生泡沫
    float foam_decay = 0.95f;         ///< 泡沫衰减率
    float caustics_intensity = 0.5f;
    float max_wave_height = 5.0f;
};

/// 海洋 tile 数据（单 tile 高度 + 法线 + 位移）
struct OceanTileData {
    std::vector<float> heights;           ///< fft_resolution × fft_resolution
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> displacement;  ///< 横向位移 (choppy)
    std::vector<float> foam;              ///< 泡沫强度 [0,1]
    float jacobian_min = 0.0f;
};

/// 海洋系统
class DSE_EXPORT OceanSystem {
public:
    OceanSystem() = default;
    ~OceanSystem() = default;

    void Init(const OceanConfig& config);
    void Shutdown();

    /// 每帧更新：驱动 FFT 模拟 + 更新 tile 数据
    void Update(float time, const glm::vec3& camera_pos);

    /// 获取指定世界坐标的海面高度
    float GetHeightAt(float world_x, float world_z) const;

    /// 获取指定世界坐标的海面法线
    glm::vec3 GetNormalAt(float world_x, float world_z) const;

    /// 获取指定世界坐标的泡沫强度
    float GetFoamAt(float world_x, float world_z) const;

    /// 获取当前可见 tile 数（用于渲染统计）
    uint32_t GetVisibleTileCount() const { return visible_tile_count_; }

    /// 获取 LOD 层级数
    int GetLODCount() const { return static_cast<int>(lod_levels_.size()); }

    /// 获取配置
    const OceanConfig& GetConfig() const { return config_; }

    /// 设置风参数（运行时可调）
    void SetWind(float speed, float dir_x, float dir_z);

    /// 设置波浪参数
    void SetChoppiness(float c) { config_.choppiness = c; }

    /// 浮动原点重定位
    void RebaseOrigin(const glm::vec3& offset);

    /// 获取统计信息
    struct OceanStats {
        uint32_t total_tiles;
        uint32_t visible_tiles;
        uint32_t fft_resolution;
        float current_max_height;
    };
    OceanStats GetStats() const;

private:
    void InitSpectrum();
    void ComputeFFT(float time);
    void UpdateFoam(float dt);
    float PhillipsSpectrum(const glm::vec2& k) const;
    float JONSWAPSpectrum(const glm::vec2& k) const;
    float SampleHeight(int grid_x, int grid_z) const;

    // Simple DFT for CPU fallback (in production would use GPU FFT)
    void DFT2D(std::vector<std::complex<float>>& data, int N, bool inverse);

    OceanConfig config_;
    std::vector<OceanLODLevel> lod_levels_;
    OceanTileData tile_data_;

    // Spectrum data
    std::vector<std::complex<float>> h0_;          ///< 初始频谱 h0(k)
    std::vector<std::complex<float>> h0_conj_;     ///< h0*(-k)
    std::vector<float> omega_;                     ///< 角频率 ω(k)
    std::vector<std::complex<float>> ht_;          ///< 时域频谱 h(k,t)
    std::vector<std::complex<float>> dx_spectrum_; ///< x位移频谱
    std::vector<std::complex<float>> dz_spectrum_; ///< z位移频谱

    glm::vec3 origin_offset_{0.0f};
    uint32_t visible_tile_count_ = 0;
    float last_time_ = 0.0f;
    bool initialized_ = false;
};

} // namespace render
} // namespace dse
