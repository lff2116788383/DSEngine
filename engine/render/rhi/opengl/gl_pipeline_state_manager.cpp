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

    // --- 娣峰悎鐘舵€?---
    // Apply blend state authoritatively on every PSO bind, for the same reason
    // as depth/cull below: raw glEnable(GL_BLEND)/glBlendFunc* calls across the
    // GL executors bypass this manager, so cached_gl_state_ does NOT reliably
    // mirror real GL state. Diffing against it can skip needed updates (e.g. the
    // default cached desc already has blend_enabled=true, so a freshly-created
    // context whose GL_BLEND is still disabled never gets glEnable -> sprite
    // alpha blending silently no-ops). Issue unconditionally to stay correct.
    if (state.blend_enabled) {
        glEnable(GL_BLEND);
        glBlendFuncSeparate(ToGLBlendFactor(state.blend_src), ToGLBlendFactor(state.blend_dst),
                            ToGLBlendFactor(state.alpha_blend_src), ToGLBlendFactor(state.alpha_blend_dst));
    } else {
        glDisable(GL_BLEND);
    }

    // --- 娣卞害娴嬭瘯 Diff ---
    // A1 fix: apply depth/cull state authoritatively on every PSO bind.
    // Many raw glDepthFunc/glDepthMask/glEnable(GL_DEPTH_TEST) calls across the
    // GL executors bypass this manager, so cached_gl_state_ does NOT reliably
    // mirror real GL state. Diffing against it can skip needed updates (e.g. the
    // skybox PSO's LEQUAL), leaving func at GL_LESS so the z=1.0 skybox is fully
    // depth-rejected -> black background. Apply unconditionally to stay correct.
    if (state.depth_test_enabled) {
        glEnable(GL_DEPTH_TEST);
    } else {
        glDisable(GL_DEPTH_TEST);
    }
    glDepthMask(state.depth_write_enabled ? GL_TRUE : GL_FALSE);
    if (state.depth_test_enabled) {
        glDepthFunc(ToGLCompareFunc(state.depth_func));
    }

    // --- 瑁佸壀闈?Diff ---
    if (state.culling_enabled) {
        glEnable(GL_CULL_FACE);
        glCullFace(ToGLCullFace(state.cull_face));
    } else {
        glDisable(GL_CULL_FACE);
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
