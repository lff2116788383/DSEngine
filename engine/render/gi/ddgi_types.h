#ifndef DSE_RENDER_GI_DDGI_TYPES_H
#define DSE_RENDER_GI_DDGI_TYPES_H

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>

namespace dse {
namespace render {
namespace gi {

// ============================================================================
// Octahedral Map 编解码
// 将单位球面方向 (unit vec3) 映射到 [0,1]^2 八面体 UV
// 参考: "A Survey of Efficient Representations for Independent Unit Vectors"
// ============================================================================

/// 将单位方向向量编码为八面体 UV [0,1]^2
inline glm::vec2 OctEncode(const glm::vec3& n) {
    float sum = std::abs(n.x) + std::abs(n.y) + std::abs(n.z);
    glm::vec2 oct = glm::vec2(n.x, n.y) / sum;
    if (n.z < 0.0f) {
        float ox = oct.x;
        float oy = oct.y;
        oct.x = (1.0f - std::abs(oy)) * (ox >= 0.0f ? 1.0f : -1.0f);
        oct.y = (1.0f - std::abs(ox)) * (oy >= 0.0f ? 1.0f : -1.0f);
    }
    // Map [-1,1] to [0,1]
    return oct * 0.5f + 0.5f;
}

/// 将八面体 UV [0,1]^2 解码为单位方向向量
inline glm::vec3 OctDecode(const glm::vec2& uv) {
    // Map [0,1] to [-1,1]
    glm::vec2 f = uv * 2.0f - 1.0f;
    glm::vec3 n(f.x, f.y, 1.0f - std::abs(f.x) - std::abs(f.y));
    if (n.z < 0.0f) {
        float nx = n.x;
        float ny = n.y;
        n.x = (1.0f - std::abs(ny)) * (nx >= 0.0f ? 1.0f : -1.0f);
        n.y = (1.0f - std::abs(nx)) * (ny >= 0.0f ? 1.0f : -1.0f);
    }
    return glm::normalize(n);
}

// ============================================================================
// Probe Grid 配置
// ============================================================================

/// DDGI Probe Volume 配置参数
struct DDGIVolumeConfig {
    glm::vec3 origin = glm::vec3(0.0f);     ///< 网格最小角（世界坐标）
    glm::vec3 extent = glm::vec3(100.0f);   ///< 网格范围（世界坐标）
    glm::ivec3 resolution = glm::ivec3(8);  ///< 各轴探针数量 (nx, ny, nz)

    /// 每个探针的辐照度 octahedral texel 分辨率（含 1 像素边界）
    int irradiance_texels = 8;

    /// 每个探针的可见性 octahedral texel 分辨率（含 1 像素边界）
    int visibility_texels = 8;

    /// 每次探针更新采样的 VPL 数量
    int rays_per_probe = 256;

    /// Temporal hysteresis（新旧数据混合因子）
    float hysteresis = 0.97f;

    /// 探针间距（自动计算）
    glm::vec3 ProbeSpacing() const {
        return glm::vec3(
            extent.x / std::max(1.0f, static_cast<float>(resolution.x - 1)),
            extent.y / std::max(1.0f, static_cast<float>(resolution.y - 1)),
            extent.z / std::max(1.0f, static_cast<float>(resolution.z - 1))
        );
    }

    /// 总探针数量
    int TotalProbeCount() const {
        return resolution.x * resolution.y * resolution.z;
    }

    /// 3D 索引 → 线性索引
    int ProbeIndex(int x, int y, int z) const {
        return x + y * resolution.x + z * resolution.x * resolution.y;
    }

    /// 线性索引 → 3D 索引
    glm::ivec3 ProbeCoord(int index) const {
        int z = index / (resolution.x * resolution.y);
        int remainder = index % (resolution.x * resolution.y);
        int y = remainder / resolution.x;
        int x = remainder % resolution.x;
        return glm::ivec3(x, y, z);
    }

    /// 获取探针世界坐标
    glm::vec3 ProbePosition(int index) const {
        glm::ivec3 coord = ProbeCoord(index);
        glm::vec3 spacing = ProbeSpacing();
        return origin + glm::vec3(coord) * spacing;
    }

    /// 计算 irradiance atlas 尺寸（2D 纹理宽高）
    /// 排列: probes_per_row × probes_per_col，每个 probe 占 irradiance_texels^2
    glm::ivec2 IrradianceAtlasSize() const {
        int probes_per_row = resolution.x * resolution.z;
        int probes_per_col = resolution.y;
        return glm::ivec2(
            probes_per_row * irradiance_texels,
            probes_per_col * irradiance_texels
        );
    }

    /// 计算 visibility atlas 尺寸
    glm::ivec2 VisibilityAtlasSize() const {
        int probes_per_row = resolution.x * resolution.z;
        int probes_per_col = resolution.y;
        return glm::ivec2(
            probes_per_row * visibility_texels,
            probes_per_col * visibility_texels
        );
    }

    /// 探针线性索引 → atlas 中的像素偏移（左上角）
    glm::ivec2 ProbeIrradianceOffset(int index) const {
        glm::ivec3 coord = ProbeCoord(index);
        int col = coord.x + coord.z * resolution.x;
        int row = coord.y;
        return glm::ivec2(col * irradiance_texels, row * irradiance_texels);
    }

    glm::ivec2 ProbeVisibilityOffset(int index) const {
        glm::ivec3 coord = ProbeCoord(index);
        int col = coord.x + coord.z * resolution.x;
        int row = coord.y;
        return glm::ivec2(col * visibility_texels, row * visibility_texels);
    }
};

// ============================================================================
// Probe 运行时状态
// ============================================================================

/// 每个探针的 GPU 状态（存入 SSBO）
struct alignas(16) ProbeState {
    glm::vec4 position_and_status;  ///< xyz=世界坐标, w=status (1.0=active, 0.0=inactive)
};

/// RSM Virtual Point Light 数据（存入 SSBO，从 shadow pass 产出）
struct alignas(16) RSMSample {
    glm::vec4 position;   ///< xyz=世界坐标, w=unused
    glm::vec4 normal;     ///< xyz=世界法线, w=unused
    glm::vec4 flux;       ///< rgb=辐射通量(albedo*light), a=unused
};

// ============================================================================
// DDGI GPU 资源句柄
// ============================================================================

struct DDGIResources {
    unsigned int irradiance_atlas = 0;       ///< RGBA16F 纹理（辐照度 octahedral atlas）
    unsigned int visibility_atlas = 0;       ///< RG16F 纹理（深度 + 深度² atlas）
    unsigned int probe_state_ssbo = 0;       ///< ProbeState[] SSBO
    unsigned int rsm_position_rt = 0;        ///< RSM 世界坐标 RT (RGBA32F)
    unsigned int rsm_normal_rt = 0;          ///< RSM 法线 RT (RGBA16F)
    unsigned int rsm_flux_rt = 0;            ///< RSM 辐射通量 RT (RGBA16F)
    unsigned int update_compute_shader = 0;  ///< 探针更新 compute shader
    bool initialized = false;
};

} // namespace gi
} // namespace render
} // namespace dse

#endif // DSE_RENDER_GI_DDGI_TYPES_H
