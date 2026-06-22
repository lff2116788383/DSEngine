/**
 * @file hair_renderer.h
 * @brief 后端无关的毛发线带渲染器（B4）。
 *
 * 用通用绘制原语（SetPipelineState / BindShaderProgram / BindUniformBuffer /
 * BindStorageBuffer / Draw）消费内建程序 BuiltinProgram::HairStrand
 * （hair.vert + hair.frag）：组合 HairUniforms UBO\@set0.b0（VS/FS 共享）+
 * position/tangent SSBO\@set7.b0/b1（@SSBO_LOW_REGISTERS → 三后端 BindStorageBuffer(0/1) 对齐）。
 * 顶点无属性（vertexless：按 gl_VertexIndex 取 SSBO）；PSO 烘焙 LINE_STRIP 拓扑，
 * 逐 strand 调 cmd.Draw(count, first) 画线带。
 *
 * 取代逐效果 ABI CommandBuffer::DrawHairStrands —— 把毛发着色器、SSBO 绑定、
 * 半透明只读深度 PSO、逐 strand 多段绘制收敛到边界之上的高层渲染器，
 * 设备/命令缓冲只保留通用原语。
 */

#ifndef DSE_RENDER_HAIR_RENDERER_H
#define DSE_RENDER_HAIR_RENDERER_H

#include <vector>

#include <glm/glm.hpp>

#include "engine/render/rhi/rhi_handle.h"
#include "engine/render/rhi/rhi_types.h"

namespace dse {
namespace render {

class CommandBuffer;
class RhiDevice;

/// 后端无关的毛发线带渲染器（B4）。半透明混合、测深度但不写深度、不剔除。
class HairRenderer {
public:
    HairRenderer() = default;
    ~HairRenderer() = default;

    HairRenderer(const HairRenderer&) = delete;
    HairRenderer& operator=(const HairRenderer&) = delete;

    /// 在当前 render pass 内绘制全部毛发系统。view/proj 为相机矩阵（proj 须含各后端裁剪修正）。
    void Draw(CommandBuffer& cmd, RhiDevice& device,
              const std::vector<HairDrawItem>& items,
              const glm::mat4& view, const glm::mat4& proj);

    /// 释放内部 GPU 资源（PSO、per-item UBO）。
    void Shutdown(RhiDevice& device);

private:
    void EnsureResources(RhiDevice& device);

    bool init_ = false;
    unsigned int pso_ = 0;
    BufferHandle hair_ubo_;
};

}  // namespace render
}  // namespace dse

#endif  // DSE_RENDER_HAIR_RENDERER_H
