/**
 * @file gl_pipeline_state_manager.cpp
 * @brief GLPipelineStateManager 实现 - 管线状态管理器（带 Diff 优化）
 */

#include "engine/render/rhi/gl_pipeline_state_manager.h"
#include "engine/render/rhi/gl_enum_convert.h"
#include "engine/base/debug.h"
#include <glad/gl.h>

namespace dse {
namespace render {

unsigned int GLPipelineStateManager::CreatePipelineState(const PipelineStateDesc& desc) {
    unsigned int handle = ++next_pipeline_state_handle_;
    pipeline_states_[handle] = desc;
    return handle;
}

const PipelineStateDesc* GLPipelineStateManager::GetPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

void GLPipelineStateManager::ApplyState(unsigned int handle) {
    // 快速路径：同一管线状态连续 Apply，直接跳过
    if (active_pipeline_state_ == handle && handle != 0) {
        ++diff_hits_;
        return;
    }

    active_pipeline_state_ = handle;

    auto it = pipeline_states_.find(handle);
    if (it == pipeline_states_.end()) {
        // 未找到状态时回退到默认混合状态
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        return;
    }

    const auto& state = it->second;
    const auto& cached = cached_gl_state_;

    // --- 混合状态 Diff ---
    if (state.blend_enabled != cached.blend_enabled) {
        if (state.blend_enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
    }
    if (state.blend_enabled && (state.blend_src != cached.blend_src || state.blend_dst != cached.blend_dst)) {
        glBlendFuncSeparate(ToGLBlendFactor(state.blend_src), ToGLBlendFactor(state.blend_dst),
                            GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    // --- 深度测试 Diff ---
    if (state.depth_test_enabled != cached.depth_test_enabled) {
        if (state.depth_test_enabled) {
            glEnable(GL_DEPTH_TEST);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
    }
    if (state.depth_write_enabled != cached.depth_write_enabled) {
        glDepthMask(state.depth_write_enabled ? GL_TRUE : GL_FALSE);
    }
    if (state.depth_test_enabled && state.depth_func != cached.depth_func) {
        glDepthFunc(ToGLCompareFunc(state.depth_func));
    }

    // --- 裁剪面 Diff ---
    if (state.culling_enabled != cached.culling_enabled) {
        if (state.culling_enabled) {
            glEnable(GL_CULL_FACE);
        } else {
            glDisable(GL_CULL_FACE);
        }
    }
    if (state.culling_enabled && state.cull_face != cached.cull_face) {
        glCullFace(ToGLCullFace(state.cull_face));
    }

    // 更新缓存
    cached_gl_state_ = state;
}

void GLPipelineStateManager::ClearActiveState() {
    active_pipeline_state_ = 0;
    // 重置缓存为默认值，确保下次 ApplyState 会完整设置所有 GL 状态
    cached_gl_state_ = PipelineStateDesc{};
}

void GLPipelineStateManager::Shutdown() {
    pipeline_states_.clear();
    active_pipeline_state_ = 0;
    cached_gl_state_ = PipelineStateDesc{};
    diff_hits_ = 0;
}

} // namespace render
} // namespace dse
