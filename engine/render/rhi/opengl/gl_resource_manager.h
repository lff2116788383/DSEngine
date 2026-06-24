/**
 * @file gl_resource_manager.h
 * @brief OpenGL GPU 资源管理器 - 负责所有 GPU 资源的创建、销毁和生命周期
 *
 * 从 OpenGLRhiDevice 中提取的第一个子系统：
 * - 纹理（Texture2D / TextureCube）
 * - 缓冲区（VBO / EBO）
 * - 帧缓冲（FBO / RenderTarget）
 * - 顶点数组（VAO）
 * - 管线状态（PipelineState）
 *
 * @note 当前阶段为接口定义 + 适配层，实现仍委托到 OpenGLRhiDevice，
 *       后续将逐步迁移实现到此管理器中
 */

#ifndef DSE_RENDER_GL_RESOURCE_MANAGER_H
#define DSE_RENDER_GL_RESOURCE_MANAGER_H

#include "engine/render/rhi/rhi_types.h"
#include <unordered_map>
#include <vector>
#include <cstddef>
#include <cstdint>

namespace dse {
namespace render {

/**
 * @struct ResourceLedger
 * @brief GPU 资源账本，跟踪各类资源的创建/销毁计数，用于泄漏检测
 */
struct ResourceLedger {
    std::size_t textures_created = 0;
    std::size_t textures_destroyed = 0;
    std::size_t framebuffers_created = 0;
    std::size_t framebuffers_destroyed = 0;
    std::size_t shader_programs_created = 0;
    std::size_t shader_programs_destroyed = 0;
    std::size_t vertex_arrays_created = 0;
    std::size_t vertex_arrays_destroyed = 0;
    std::size_t buffers_created = 0;
    std::size_t buffers_destroyed = 0;
    std::size_t render_targets_created = 0;
    std::size_t render_targets_destroyed = 0;
    std::size_t pipeline_states_created = 0;
    std::size_t pipeline_states_destroyed = 0;
};

/**
 * @struct RenderTargetResource
 * @brief 渲染目标资源的内部存储结构
 */
struct RenderTargetResource {
    RenderTargetDesc desc;
    unsigned int fbo_handle = 0;                ///< 渲染用 FBO（MSAA 时为多重采样 FBO）
    unsigned int color_texture_handle = 0;      ///< 兼容：等于 color_texture_handles[0]
    unsigned int depth_texture_handle = 0;
    std::vector<unsigned int> color_texture_handles; ///< MRT: 所有颜色附件纹理句柄

    // --- MSAA（A6 多重采样）---
    // msaa_samples > 1 时：fbo_handle 指向多重采样 FBO（color/depth 用 renderbuffer），
    // 采样/读回所需的单采样纹理挂在 resolve_fbo_handle 上，EndRenderPass 经 glBlitFramebuffer
    // 把多重采样附件解析到这些纹理。msaa_samples == 1 时全部为 0（保持原行为）。
    unsigned int resolve_fbo_handle = 0;             ///< 单采样解析 FBO（持有 color/depth 纹理附件）
    int msaa_samples = 1;                            ///< 有效采样数（已按 GL_MAX_SAMPLES clamp）
    std::vector<unsigned int> msaa_color_rb_handles; ///< 多重采样颜色 renderbuffer
    unsigned int msaa_depth_rb_handle = 0;           ///< 多重采样深度 renderbuffer
};

/**
 * @class GLResourceManager
 * @brief OpenGL GPU 资源管理器
 *
 * 职责：
 * 1. 纹理创建/销毁/查询
 * 2. 缓冲区创建/更新/销毁
 * 3. 帧缓冲创建/销毁/读取
 * 4. 顶点数组创建/销毁
 * 5. 管线状态创建/查询
 * 6. 资源泄漏检测（ResourceLedger）
 *
 * @note 过渡阶段：此类持有资源数据，但 GL 调用暂时委托到 OpenGLRhiDevice。
 *       后续将逐步把 glCreateTextures/glCreateBuffers 等迁移到此管理器。
 */
class GLResourceManager {
public:
    GLResourceManager() = default;
    ~GLResourceManager() = default;

    // --- 句柄生成 ---
    unsigned int AllocateRenderTargetHandle();
    unsigned int AllocateTextureHandle();
    unsigned int AllocatePipelineStateHandle();

    // --- 渲染目标 ---
    void StoreRenderTarget(unsigned int handle, const RenderTargetResource& rt);
    const RenderTargetResource* GetRenderTarget(unsigned int handle) const;
    void RemoveRenderTarget(unsigned int handle);
    /// 销毁所有渲染目标（由 OpenGLRhiDevice::Shutdown 调用）
    void DestroyAllRenderTargets();

    // --- 管线状态 ---
    void StorePipelineState(unsigned int handle, const PipelineStateDesc& desc);
    const PipelineStateDesc* GetPipelineState(unsigned int handle) const;

    // --- 资源账本 ---
    ResourceLedger& ledger();
    const ResourceLedger& ledger() const;

    /// 打印资源账本统计信息，用于检查内存泄漏
    void LogResourceLedger() const;

private:
    // 句柄计数器
    unsigned int next_render_target_handle_ = 320000;
    unsigned int next_texture_handle_ = 340000;
    unsigned int next_pipeline_state_handle_ = 330000;

    // 资源存储
    std::unordered_map<unsigned int, RenderTargetResource> render_targets_;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;

    ResourceLedger resource_ledger_;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_RESOURCE_MANAGER_H
