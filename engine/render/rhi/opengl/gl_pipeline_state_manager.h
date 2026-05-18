/**
 * @file gl_pipeline_state_manager.h
 * @brief OpenGL 管线状态管理器 - 负责渲染状态（混合/深度/剔除）的缓存与应用
 *
 * 从 OpenGLRhiDevice 中提取的第二个子系统：
 * - 管线状态创建/查询/销毁
 * - 活跃状态追踪，避免冗余 GL 调用
 * - 状态 Diff 机制（后续可扩展为完整状态快照/恢复）
 */

#ifndef DSE_RENDER_GL_PIPELINE_STATE_MANAGER_H
#define DSE_RENDER_GL_PIPELINE_STATE_MANAGER_H

#include "engine/render/rhi/rhi_types.h"
#include <unordered_map>

namespace dse {
namespace render {

/**
 * @class GLPipelineStateManager
 * @brief OpenGL 管线状态管理器
 *
 * 职责：
 * 1. 管线状态描述的创建、存储、查询
 * 2. 将 PipelineStateDesc 应用到 OpenGL 状态机（带 Diff 优化）
 * 3. 追踪当前活跃状态，仅切换变化的部分以减少冗余 GL 调用
 */
class GLPipelineStateManager {
public:
    GLPipelineStateManager() = default;
    ~GLPipelineStateManager() = default;

    /// 创建管线状态并返回句柄
    unsigned int CreatePipelineState(const PipelineStateDesc& desc);

    /// 查询管线状态描述（不存在时返回 nullptr）
    const PipelineStateDesc* GetPipelineState(unsigned int handle) const;

    /// 将管线状态应用到 OpenGL（带 Diff：仅设置与当前活跃状态不同的部分）
    void ApplyState(unsigned int handle);

    /// 清除活跃状态追踪（窗口重建/上下文丢失后调用）
    void ClearActiveState();

    /// 销毁所有管线状态资源
    void Shutdown();

    // --- 访问器 ---
    unsigned int active_pipeline_state() const { return active_pipeline_state_; }
    void set_active_pipeline_state(unsigned int handle) { active_pipeline_state_ = handle; }

    const std::unordered_map<unsigned int, PipelineStateDesc>& pipeline_states() const { return pipeline_states_; }

    /// 上一次 ApplyState 时的 Diff 命中次数（同一 handle 连续 Apply 跳过）
    std::size_t diff_hits() const { return diff_hits_; }

    /// 当前管线状态数量（供 Shutdown 时更新账本）
    std::size_t pipeline_state_count() const { return pipeline_states_.size(); }

private:
    unsigned int next_pipeline_state_handle_ = 330000;
    std::unordered_map<unsigned int, PipelineStateDesc> pipeline_states_;
    unsigned int active_pipeline_state_ = 0;

    /// 当前 GL 状态缓存（与上一次 ApplyState 对应的 PipelineStateDesc）
    PipelineStateDesc cached_gl_state_;

    /// Diff 统计
    std::size_t diff_hits_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_GL_PIPELINE_STATE_MANAGER_H
