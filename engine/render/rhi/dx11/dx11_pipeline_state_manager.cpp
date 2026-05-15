/**
 * @file dx11_pipeline_state_manager.cpp
 * @brief DX11PipelineStateManager 实现 — BlendState/DepthStencilState/RasterizerState
 */

#include "engine/render/rhi/dx11/dx11_pipeline_state_manager.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

void DX11PipelineStateManager::Init(DX11Context* context) {
    context_ = context;
    DEBUG_LOG_INFO("[D3D11] PipelineStateManager initialized");
}

void DX11PipelineStateManager::Shutdown() {
    pipeline_states_.clear();
    active_pipeline_state_ = 0;
    DEBUG_LOG_INFO("[D3D11] PipelineStateManager shutdown");
}

D3D11_BLEND DX11PipelineStateManager::ToD3D11Blend(BlendFactor factor) {
    switch (factor) {
        case BlendFactor::Zero:             return D3D11_BLEND_ZERO;
        case BlendFactor::One:              return D3D11_BLEND_ONE;
        case BlendFactor::SrcAlpha:         return D3D11_BLEND_SRC_ALPHA;
        case BlendFactor::OneMinusSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
        case BlendFactor::DstAlpha:         return D3D11_BLEND_DEST_ALPHA;
        case BlendFactor::OneMinusDstAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
        default: return D3D11_BLEND_ONE;
    }
}

D3D11_COMPARISON_FUNC DX11PipelineStateManager::ToD3D11ComparisonFunc(CompareFunc func) {
    switch (func) {
        case CompareFunc::Never:        return D3D11_COMPARISON_NEVER;
        case CompareFunc::Less:         return D3D11_COMPARISON_LESS;
        case CompareFunc::Equal:        return D3D11_COMPARISON_EQUAL;
        case CompareFunc::LessEqual:    return D3D11_COMPARISON_LESS_EQUAL;
        case CompareFunc::Greater:      return D3D11_COMPARISON_GREATER;
        case CompareFunc::NotEqual:     return D3D11_COMPARISON_NOT_EQUAL;
        case CompareFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
        case CompareFunc::Always:       return D3D11_COMPARISON_ALWAYS;
        default: return D3D11_COMPARISON_LESS;
    }
}

D3D11_CULL_MODE DX11PipelineStateManager::ToD3D11CullMode(CullFace face) {
    switch (face) {
        case CullFace::None:  return D3D11_CULL_NONE;
        case CullFace::Front: return D3D11_CULL_FRONT;
        case CullFace::Back:  return D3D11_CULL_BACK;
        default: return D3D11_CULL_BACK;
    }
}

unsigned int DX11PipelineStateManager::CreatePipelineState(const PipelineStateDesc& desc) {
    if (!context_) return 0;

    DX11PipelineState state;
    state.desc = desc;

    ID3D11Device* device = context_->device();

    // Blend State
    D3D11_BLEND_DESC blend_desc{};
    blend_desc.RenderTarget[0].BlendEnable = desc.blend_enabled ? TRUE : FALSE;
    blend_desc.RenderTarget[0].SrcBlend = ToD3D11Blend(desc.blend_src);
    blend_desc.RenderTarget[0].DestBlend = ToD3D11Blend(desc.blend_dst);
    blend_desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].SrcBlendAlpha = ToD3D11Blend(desc.alpha_blend_src);
    blend_desc.RenderTarget[0].DestBlendAlpha = ToD3D11Blend(desc.alpha_blend_dst);
    blend_desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blend_desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = device->CreateBlendState(&blend_desc, state.blend_state.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateBlendState failed");
        return 0;
    }

    // Depth Stencil State
    D3D11_DEPTH_STENCIL_DESC ds_desc{};
    ds_desc.DepthEnable = desc.depth_test_enabled ? TRUE : FALSE;
    ds_desc.DepthWriteMask = desc.depth_write_enabled ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
    ds_desc.DepthFunc = ToD3D11ComparisonFunc(desc.depth_func);
    ds_desc.StencilEnable = FALSE;

    hr = device->CreateDepthStencilState(&ds_desc, state.depth_stencil_state.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateDepthStencilState failed");
        return 0;
    }

    // Rasterizer State
    D3D11_RASTERIZER_DESC rast_desc{};
    rast_desc.FillMode = D3D11_FILL_SOLID;
    rast_desc.CullMode = ToD3D11CullMode(desc.cull_face);
    rast_desc.FrontCounterClockwise = TRUE;
    rast_desc.DepthBias = 0;
    rast_desc.DepthBiasClamp = 0.0f;
    rast_desc.SlopeScaledDepthBias = 0.0f;
    rast_desc.DepthClipEnable = TRUE;
    rast_desc.ScissorEnable = FALSE;
    rast_desc.MultisampleEnable = FALSE;
    rast_desc.AntialiasedLineEnable = FALSE;

    hr = device->CreateRasterizerState(&rast_desc, state.rasterizer_state.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateRasterizerState failed");
        return 0;
    }

    unsigned int handle = next_handle_++;
    pipeline_states_[handle] = std::move(state);
    return handle;
}

const DX11PipelineState* DX11PipelineStateManager::GetPipelineState(unsigned int handle) const {
    auto it = pipeline_states_.find(handle);
    return it != pipeline_states_.end() ? &it->second : nullptr;
}

void DX11PipelineStateManager::ApplyPipelineState(unsigned int handle, ID3D11DeviceContext* dc) {
    auto it = pipeline_states_.find(handle);
    if (it == pipeline_states_.end()) return;

    auto& state = it->second;
    float blend_factor[4] = {0, 0, 0, 0};
    dc->OMSetBlendState(state.blend_state.Get(), blend_factor, 0xffffffff);
    dc->OMSetDepthStencilState(state.depth_stencil_state.Get(), 0);
    dc->RSSetState(state.rasterizer_state.Get());

    active_pipeline_state_ = handle;
}

} // namespace render
} // namespace dse
