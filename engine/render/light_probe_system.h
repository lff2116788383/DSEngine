/**
 * @file light_probe_system.h
 * @brief Light Probe SH Bake 系统 — 运行时球谐（SH L2）积分与查询
 *
 * 功能：
 * - 对每个 LightProbeComponent 所在位置渲染 6 面 cubemap → CPU 回读 → 积分 SH L2（9 个 RGB 系数）
 * - 运行时按物体位置查询最近 probe，传 SH 系数到 UBO
 * - 支持 probe 距离加权混合（可选）
 */

#ifndef DSE_RENDER_LIGHT_PROBE_SYSTEM_H
#define DSE_RENDER_LIGHT_PROBE_SYSTEM_H

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

#include "engine/render/skybox_renderer.h"

class World;
class AssetManager;

namespace dse {
namespace render {

class RhiDevice;
class CommandBuffer;
struct RenderPassContext;
struct RenderSceneView;

/// SH L2 系数（9 个 vec3，RGB 通道）
struct SHL2 {
    glm::vec3 coeffs[9] = {};
};

/// 烘焙好的 probe 数据（CPU 端缓存）
struct BakedProbe {
    glm::vec3 position = glm::vec3(0.0f);
    float influence_radius = 10.0f;
    SHL2 sh;
};

/**
 * @class LightProbeSystem
 * @brief 运行时 Light Probe SH 烘焙与查询
 */
class LightProbeSystem {
public:
    LightProbeSystem() = default;
    ~LightProbeSystem() = default;

    /// 初始化（创建 cubemap RT）
    void Init(RhiDevice* rhi_device);

    /// 销毁资源
    void Shutdown();

    /// 对场景中所有 needs_rebake=true 的 probe 执行 bake
    /// 需在渲染帧外调用（或帧末），因为要渲染 6 面 cubemap + 回读
    void BakePendingProbes(World& world, RhiDevice* rhi_device,
                           RenderPassContext& ctx);

    /// 运行时查询：根据相机位置选择最近 probe 的 SH，写入 RHI 全局状态（ECS-free）
    void UpdateGlobalSH(const RenderSceneView& scene_view, RhiDevice* rhi_device,
                        const glm::vec3& camera_position);

    /// 获取已烘焙的 probe 列表（调试用）
    const std::vector<BakedProbe>& baked_probes() const { return baked_probes_; }

    /// 对单个位置渲染 cubemap 并积分 SH L2（CPU 端）
    static SHL2 BakeSHAtPosition(const glm::vec3& position, int face_resolution,
                                  RhiDevice* rhi_device, unsigned int cubemap_rt,
                                  RenderPassContext& ctx,
                                  SkyboxRenderer* skybox_renderer = nullptr);

    /// 从 RGBA8 像素数据积分单面 SH（CPU 端纯计算）
    static void IntegrateFaceSH(const unsigned char* rgba8, int width, int height,
                                 int face_index, SHL2& out_sh);

private:
    unsigned int cubemap_rt_ = 0;       ///< 用于 bake 的 cubemap 渲染目标
    int face_resolution_ = 64;          ///< cubemap 单面分辨率
    std::vector<BakedProbe> baked_probes_;
    SkyboxRenderer skybox_renderer_;    ///< A1：天空盒用通用原语绘制（取代 DrawSkybox）
    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_LIGHT_PROBE_SYSTEM_H
