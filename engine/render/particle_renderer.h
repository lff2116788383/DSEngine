/**
 * @file particle_renderer.h
 * @brief 后端无关的 3D 粒子广告牌渲染器（B3）。
 *
 * 用通用绘制原语（BindPipeline / BindUniformBuffer /
 * BindStorageBuffer / BindTexture / BindVertexBuffer / BindIndexBuffer /
 * DrawIndexedInstanced）消费内建程序 BuiltinProgram::Particle3D
 * （particle_instanced.vert + particle.frag）：共享单位 quad VB/IB + PerFrame UBO（vp/view），
 * 每实例 pos/size/color 走 SSBO\@slot 0（set7.b0），VS 按 gl_InstanceIndex 取数 + view 轴广告牌。
 *
 * 取代逐效果 ABI CommandBuffer::DrawParticles3D —— 把四边形几何、实例 SSBO 绑定、
 * 加性混合 PSO 收敛到边界之上的高层渲染器，设备/命令缓冲只保留通用原语。
 */

#ifndef DSE_RENDER_PARTICLE_RENDERER_H
#define DSE_RENDER_PARTICLE_RENDERER_H

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "engine/render/rhi/rhi_handle.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/// 单个 3D 粒子系统的绘制项（高层渲染器输入，非 ABI 类型）。
/// instance_buffer 为每实例 std430 SSBO 的底层句柄，内容布局须与 particle_instanced.vert
/// 的 ParticleInstance 一致：{ vec4 pos_size(xyz=world pos, w=size); vec4 color; }[]。
/// 由发射系统（particle3d / fluid）经 CreateGpuBuffer(kStorage) 创建并每帧填充，
/// ParticleRenderer 仅绑定、不拷贝。
struct ParticleDrawItem {
    unsigned int texture_handle = 0;   ///< u_texture（0 → 回退内建 1x1 白纹理）
    unsigned int instance_buffer = 0;  ///< 每实例 pos/size/color SSBO 底层句柄
    int particle_count = 0;            ///< 活跃粒子数（= 实例数）
};

/// 后端无关的 3D 粒子广告牌渲染器（B3）。加性混合、测深度但不写深度、不剔除。
class ParticleRenderer {
public:
    ParticleRenderer() = default;
    ~ParticleRenderer() = default;

    ParticleRenderer(const ParticleRenderer&) = delete;
    ParticleRenderer& operator=(const ParticleRenderer&) = delete;

    /// 在当前 render pass 内绘制全部粒子系统。view/proj 为相机矩阵（proj 须含各后端裁剪修正）。
    void DrawParticles(CommandBuffer& cmd, RhiDevice& device,
                       const std::vector<ParticleDrawItem>& items,
                       const glm::mat4& view, const glm::mat4& proj);

    /// 释放内部 GPU 资源（quad VB/IB、PerFrame UBO、PSO、白纹理）。
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);

    bool init_ = false;
    unsigned int pso_ = 0;
    unsigned int white_tex_ = 0;
    BufferHandle quad_vbo_;
    BufferHandle quad_ibo_;
    BufferHandle per_frame_ubo_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_PARTICLE_RENDERER_H
