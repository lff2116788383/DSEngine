/**
 * @file meshlet_render_pass.h
 * @brief Meshlet Cluster 渲染管线 Pass — 接入 FramePipeline RenderGraph
 *
 * 职责：
 * 1. 每帧从 ECS 收集 MeshletMeshComponent 实例
 * 2. 上传 meshlet GPU 数据到 SSBO
 * 3. 执行 per-cluster GPU 剔除（视锥 + 法线锥 + Hi-Z）
 * 4. 输出 MultiDrawElementsIndirect 命令
 * 5. 绑定 Mega Buffer VAO + 材质 SSBO 执行间接绘制
 */

#ifndef DSE_MESHLET_RENDER_PASS_H
#define DSE_MESHLET_RENDER_PASS_H

#include "engine/render/passes/render_pass_interface.h"
#include "engine/render/passes/render_pass_context.h"
#include "engine/render/meshlet/meshlet_cull_pass.h"
#include "engine/render/meshlet/meshlet_types.h"
#include <memory>
#include <vector>

namespace dse {
namespace render {

// Forward declarations
class RenderGraph;
class CommandBuffer;

/// Per-meshlet 材质描述（GPU 侧 std430 布局）
struct MeshletMaterialEntry {
    uint32_t material_index;    ///< 引用全局材质数组的索引
    uint32_t pad[3];            ///< 16-byte 对齐
};
static_assert(sizeof(MeshletMaterialEntry) == 16, "MeshletMaterialEntry must be 16 bytes");

/**
 * @class MeshletCullRenderPass
 * @brief Meshlet 剔除 Pass（Compute Shader / CPU fallback）
 *
 * 在 RenderGraph 中位于 HiZ Build 之后、Forward Scene 之前。
 * 读取 hiz_mip_chain，写出 meshlet_visibility（indirect draw buffer）。
 */
class MeshletCullRenderPass : public IRenderPass {
public:
    explicit MeshletCullRenderPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "meshlet_cull"; }

    /// 获取内部 MeshletCullPass 实例（供外部注册 mesh）
    MeshletCullPass& GetCullPass() { return cull_pass_; }
    const MeshletCullPass& GetCullPass() const { return cull_pass_; }

private:
    void EnsureShader();
    void EnsureBuffers(uint32_t meshlet_count);
    void CollectInstances();
    void UploadGPUData();
    void DispatchGPUCull();
    void ExecuteCPUFallback();

    RenderPassContext& ctx_;
    MeshletCullPass cull_pass_;
    MeshletCullConfig cull_config_;

    bool shader_compiled_ = false;
    size_t gpu_data_capacity_ = 0;      ///< 当前 SSBO 容量（meshlet 数）
    size_t draw_cmd_capacity_ = 0;      ///< 当前 draw cmd SSBO 容量
    size_t material_capacity_ = 0;      ///< 当前材质 SSBO 容量

    /// Per-frame 材质映射
    std::vector<MeshletMaterialEntry> material_entries_;
};

/**
 * @class MeshletDrawRenderPass
 * @brief Meshlet 间接绘制 Pass
 *
 * 使用 MeshletCullRenderPass 的剔除结果执行 MultiDrawElementsIndirect。
 * 在 RenderGraph 中位于 Forward Scene Pass 内部或紧随其后。
 */
class MeshletDrawRenderPass : public IRenderPass {
public:
    explicit MeshletDrawRenderPass(RenderPassContext& ctx) : ctx_(ctx) {}
    void Setup(RenderGraph& graph) override;
    void Execute(CommandBuffer& cmd_buffer) override;
    const char* GetName() const override { return "meshlet_draw"; }

private:
    void EnsureMegaBuffer();

    RenderPassContext& ctx_;
    bool mega_buffer_initialized_ = false;
    size_t mega_vbo_capacity_ = 0;
    size_t mega_ibo_capacity_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_MESHLET_RENDER_PASS_H
