/**
 * @file reflection_probe_system.h
 * @brief Reflection Probe 系统 — 运行时 cubemap bake + 预滤波 + BRDF LUT
 *
 * 功能：
 * - 对每个 ReflectionProbeComponent 渲染 cubemap → 生成预滤波 mipmap（Split-Sum IBL）
 * - 预计算 BRDF LUT（引擎初始化时一次性生成）
 * - 运行时按物体位置选择最近 probe cubemap，传给 PBR shader
 */

#ifndef DSE_RENDER_REFLECTION_PROBE_SYSTEM_H
#define DSE_RENDER_REFLECTION_PROBE_SYSTEM_H

#include <glm/glm.hpp>
#include <vector>

class World;
class RhiDevice;
class CommandBuffer;

namespace dse {
namespace render {

struct RenderPassContext;

/**
 * @class ReflectionProbeSystem
 * @brief 运行时 Reflection Probe + IBL 管理
 */
class ReflectionProbeSystem {
public:
    ReflectionProbeSystem() = default;
    ~ReflectionProbeSystem() = default;

    /// 初始化（生成 BRDF LUT 纹理）
    void Init(RhiDevice* rhi_device);

    /// 销毁资源
    void Shutdown(RhiDevice* rhi_device);

    /// 对场景中 needs_rebake 的 reflection probe 执行 bake + 预滤波
    void BakePendingProbes(World& world, RhiDevice* rhi_device,
                           RenderPassContext& ctx);

    /// 运行时查询：选择最近 probe 的预滤波 cubemap，返回 handle（0 = 无可用 probe）
    unsigned int QueryNearestProbeCubemap(World& world, const glm::vec3& position) const;

    /// 获取 BRDF LUT 纹理句柄
    unsigned int brdf_lut_handle() const { return brdf_lut_handle_; }

    /// 获取 IBL 是否可用（至少有 1 个 baked probe + BRDF LUT）
    bool IsIBLAvailable() const { return brdf_lut_handle_ != 0 && !baked_cubemaps_.empty(); }

private:
    /// 生成 BRDF Integration LUT（512×512 RG16F 近似为 RGBA8）
    void GenerateBRDFLUT(RhiDevice* rhi_device);

    /// 对 cubemap 执行预滤波（在 CPU 端逐 mip 降采样 + 粗糙度卷积）
    unsigned int PrefilterCubemap(RhiDevice* rhi_device, unsigned int src_cubemap_rt,
                                   int base_resolution);

    unsigned int brdf_lut_handle_ = 0;
    unsigned int bake_rt_ = 0;             ///< 单面渲染 RT
    int bake_resolution_ = 128;

    struct ProbeEntry {
        glm::vec3 position;
        float influence_radius;
        unsigned int prefiltered_cubemap;  ///< GPU 预滤波 cubemap 纹理
    };
    std::vector<ProbeEntry> baked_cubemaps_;
    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_REFLECTION_PROBE_SYSTEM_H
