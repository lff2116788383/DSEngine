#ifndef DSE_RENDER_GI_DDGI_SYSTEM_H
#define DSE_RENDER_GI_DDGI_SYSTEM_H

#include "engine/render/gi/ddgi_types.h"
#include <string>

class RhiDevice;
class World;

namespace dse {
namespace render {
namespace gi {

/**
 * @class DDGISystem
 * @brief DDGI Irradiance Probe 系统
 *
 * 生命周期：由 FramePipeline 持有，在 Init/Shutdown 中管理 GPU 资源。
 * 每帧流程：
 *   1. PrepareRSM() — 在 shadow pass 前调用，确保 RSM RT 存在
 *   2. UpdateProbes() — Compute Shader 从 RSM VPL 更新探针辐照度/可见性
 *   3. BindForSampling() — 将 atlas 纹理绑定供 PBR shader 采样
 */
class DDGISystem {
public:
    DDGISystem() = default;
    ~DDGISystem() = default;

    /// 初始化 GPU 资源（atlas 纹理、SSBO、compute shader）
    /// @return true 如果初始化成功
    bool Init(RhiDevice* rhi, const DDGIVolumeConfig& config);

    /// 释放所有 GPU 资源
    void Shutdown(RhiDevice* rhi);

    /// 更新配置（会重建 GPU 资源）
    void Reconfigure(RhiDevice* rhi, const DDGIVolumeConfig& config);

    /// 确保 RSM RenderTarget 存在（在 CSM shadow pass 前调用）
    void EnsureRSMResources(RhiDevice* rhi);

    /// Compute Shader 探针更新（在 shadow pass 之后、forward pass 之前调用）
    /// @param rsm_position/rsm_normal/rsm_flux RSM MRT 纹理句柄
    /// @param rsm_width/rsm_height  RSM 纹理尺寸
    /// @param light_dir  主方向光方向（归一化）
    /// @param light_color 主方向光颜色 * 强度
    void UpdateProbes(RhiDevice* rhi,
                      unsigned int rsm_position, unsigned int rsm_normal, unsigned int rsm_flux,
                      int rsm_width, int rsm_height,
                      const glm::vec3& light_dir, const glm::vec3& light_color);

    /// 将 irradiance/visibility atlas 绑定到指定纹理单元供 PBR shader 采样
    void BindForSampling(RhiDevice* rhi, unsigned int irradiance_unit, unsigned int visibility_unit) const;

    /// 获取当前配置（PBR shader 需要 grid 参数）
    const DDGIVolumeConfig& GetConfig() const { return config_; }

    /// 获取 GPU 资源句柄
    const DDGIResources& GetResources() const { return resources_; }

    /// 是否已初始化
    bool IsInitialized() const { return resources_.initialized; }

    /// 获取当前帧更新的探针起始索引（用于级联轮转）
    int GetCurrentUpdateOffset() const { return current_update_offset_; }

    /// 每帧更新的探针数量
    int GetProbesPerFrame() const { return probes_per_frame_; }

private:
    bool CreateAtlasTextures(RhiDevice* rhi);
    bool CreateComputeShader(RhiDevice* rhi);
    void InitProbeStates(RhiDevice* rhi);

    DDGIVolumeConfig config_;
    DDGIResources resources_;

    int current_update_offset_ = 0;   ///< 级联轮转偏移
    int probes_per_frame_ = 32;       ///< 每帧更新探针数（可调）
    int frame_counter_ = 0;
};

} // namespace gi
} // namespace render
} // namespace dse

#endif // DSE_RENDER_GI_DDGI_SYSTEM_H
