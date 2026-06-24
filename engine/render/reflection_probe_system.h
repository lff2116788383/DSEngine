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
#include <array>
#include <vector>

#include "engine/render/skybox_renderer.h"

class World;

namespace dse {
namespace render {

class RhiDevice;
class CommandBuffer;

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

    /// 运行时着色器以 textureLod(roughness * kMaxReflectionLod) 采样预滤波 cubemap，
    /// 须与 lighting_utils.glsl 中的 MAX_REFLECTION_LOD 保持一致。
    static constexpr float kMaxReflectionLod = 4.0f;

    /// CPU 端预滤波环境贴图的 mip 链结果（6 面 RGBA8 / 每 mip）。
    /// mip0 为锐利 base（roughness 0），后续 mip 按 GGX 重要性采样卷积（roughness
    /// = min(1, mip / kMaxReflectionLod)），供运行时 split-sum IBL 高光采样。
    struct PrefilteredCube {
        int base_resolution = 0;
        int num_mips = 0;
        /// mips[mip][face] = (res>>mip)^2 * 4 字节 RGBA8
        std::vector<std::array<std::vector<unsigned char>, 6>> mips;
    };

    /// 由 6 面 base RGBA8 计算预滤波 mip 链（纯 CPU，无 GPU 依赖，可单测）。
    /// faces[face] 长度须为 res*res*4；face 顺序为 +X,-X,+Y,-Y,+Z,-Z。
    static PrefilteredCube ComputePrefilteredCube(const unsigned char* const faces[6], int res);

private:
    /// 生成 BRDF Integration LUT（512×512 RG16F 近似为 RGBA8）
    void GenerateBRDFLUT(RhiDevice* rhi_device);

    /// 对 base 6 面执行 CPU 预滤波并上传为带 mip 链的 cubemap，返回纹理 handle（0=失败）
    unsigned int PrefilterAndUploadCubemap(RhiDevice* rhi_device,
                                           const unsigned char* const faces[6], int res);

    unsigned int brdf_lut_handle_ = 0;
    unsigned int bake_rt_ = 0;             ///< 单面渲染 RT
    int bake_resolution_ = 128;
    SkyboxRenderer skybox_renderer_;       ///< A1：天空盒用通用原语绘制（取代 DrawSkybox）

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
