/**
 * @file impostor_renderer.h
 * @brief 后端无关的 Impostor LOD billboard 渲染器。
 *
 * 与 ParticleRenderer 同模式：共享单位 quad VB/IB + PerFrame UBO + per-instance SSBO。
 * 每帧由 ImpostorSystem 填充实例数据，在 ForwardScenePass Opaque 阶段绘制。
 *
 * 功能：
 * - 从相机方向计算最佳 atlas 帧索引（八面体/半球映射）
 * - 朝相机 billboard 渲染（view 轴对齐）
 * - Alpha test 剔除空白区域
 * - 可选法线图光照（减少"纸片"感）
 * - 距离渐变（transition 区间）
 */

#ifndef DSE_RENDER_IMPOSTOR_RENDERER_H
#define DSE_RENDER_IMPOSTOR_RENDERER_H

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/// 单个 impostor 实例的绘制数据（CPU 侧，上传前）
struct ImpostorDrawInstance {
    glm::vec3 world_position{0.0f};
    float half_size = 1.0f;
    int frame_x = 0;           ///< Atlas 中选取的帧列
    int frame_y = 0;           ///< Atlas 中选取的帧行
    int frames_x_total = 12;
    int frames_y_total = 3;
    glm::vec3 pivot_offset{0.0f};
    float fade = 1.0f;         ///< 距离渐变因子 [0,1]
};

/// 一批同 atlas 的 impostor 绘制项
struct ImpostorBatchItem {
    unsigned int atlas_texture = 0;         ///< albedo atlas GPU 句柄
    unsigned int normal_atlas_texture = 0;  ///< normal atlas GPU 句柄（0=不使用法线）
    float normal_strength = 1.0f;
    float alpha_cutoff = 0.5f;
    std::vector<ImpostorDrawInstance> instances;
};

/// 后端无关的 Impostor billboard 渲染器
class ImpostorRenderer {
public:
    ImpostorRenderer() = default;
    ~ImpostorRenderer() = default;

    ImpostorRenderer(const ImpostorRenderer&) = delete;
    ImpostorRenderer& operator=(const ImpostorRenderer&) = delete;

    /// 绘制全部 impostor 批次。在 ForwardScenePass Opaque 阶段调用。
    void DrawImpostors(CommandBuffer& cmd, RhiDevice& device,
                       const std::vector<ImpostorBatchItem>& batches,
                       const glm::mat4& view, const glm::mat4& proj,
                       const glm::vec3& camera_pos,
                       const glm::vec3& light_dir,
                       const glm::vec3& ambient_color);

    /// 释放内部 GPU 资源
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);

    bool init_ = false;
    unsigned int pso_ = 0;
    unsigned int white_tex_ = 0;
    BufferHandle quad_vbo_;
    BufferHandle quad_ibo_;
    BufferHandle per_frame_ubo_;
    BufferHandle params_ubo_;
    BufferHandle instance_ssbo_;
    int instance_ssbo_capacity_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_IMPOSTOR_RENDERER_H
