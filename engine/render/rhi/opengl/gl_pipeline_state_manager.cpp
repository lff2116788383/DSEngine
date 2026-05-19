/**
 * @file gl_pipeline_state_manager.cpp
 * @brief GLPipelineStateManager 瀹炵幇 - 绠＄嚎鐘舵€佺鐞嗗櫒锛堝甫 Diff 浼樺寲锛?
 */

#include "engine/render/rhi/opengl/gl_pipeline_state_manager.h"
#include "engine/render/rhi/opengl/gl_enum_convert.h"
#include "engine/base/debug.h"
#include "engine/render/rhi/opengl/gl_loader.h"

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
    // 蹇€熻矾寰勶細鍚屼竴绠＄嚎鐘舵€佽繛缁?Apply锛岀洿鎺ヨ烦杩?
    if (active_pipeline_state_ == handle && handle != 0) {
        ++diff_hits_;
        return;
    }

    active_pipeline_state_ = handle;

    auto it = pipeline_states_.find(handle);
    if (it == pipeline_states_.end()) {
        // 鏈壘鍒扮姸鎬佹椂鍥為€€鍒伴粯璁ゆ贩鍚堢姸鎬?
        glEnable(GL_BLEND);
        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        return;
    }

    const auto& state = it->second;
    const auto& cached = cached_gl_state_;

    // --- 娣峰悎鐘舵€?Diff ---
    if (state.blend_enabled != cached.blend_enabled) {
        if (state.blend_enabled) {
            glEnable(GL_BLEND);
        } else {
            glDisable(GL_BLEND);
        }
    }
    if (state.blend_enabled && (state.blend_src != cached.blend_src || state.blend_dst != cached.blend_dst
        || state.alpha_blend_src != cached.alpha_blend_src || state.alpha_blend_dst != cached.alpha_blend_dst)) {
        glBlendFuncSeparate(ToGLBlendFactor(state.blend_src), ToGLBlendFactor(state.blend_dst),
                            ToGLBlendFactor(state.alpha_blend_src), ToGLBlendFactor(state.alpha_blend_dst));
    }

    // --- 娣卞害娴嬭瘯 Diff ---
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

    // --- 瑁佸壀闈?Diff ---
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

    // 鏇存柊缂撳瓨
    cached_gl_state_ = state;
}

void GLPipelineStateManager::ClearActiveState() {
    active_pipeline_state_ = 0;
    // 閲嶇疆缂撳瓨涓洪粯璁ゅ€硷紝纭繚涓嬫 ApplyState 浼氬畬鏁磋缃墍鏈?GL 鐘舵€?
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
