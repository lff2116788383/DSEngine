/**
 * @file dx11_pipeline_state_manager.h
 * @brief D3D11 管线状态管理器 — BlendState/DepthStencilState/RasterizerState 缓存
 *
 * 将 PipelineStateDesc 转换为 D3D11 状态对象并缓存复用。
 */

#ifndef DSE_RENDER_DX11_PIPELINE_STATE_MANAGER_H
#define DSE_RENDER_DX11_PIPELINE_STATE_MANAGER_H

#include "engine/render/rhi/rhi_types.h"
#include <d3d11.h>
#include <wrl/client.h>
#include <unordered_map>

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

class DX11Context;

/// D3D11 管线状态封装
struct DX11PipelineState {
    PipelineStateDesc desc;
    ComPtr<ID3D11BlendState> blend_state;
    ComPtr<ID3D11DepthStencilState> depth_stencil_state;
    ComPtr<ID3D11RasterizerState> rasterizer_state;
};

/**
 * @class DX11PipelineStateManager
 * @brief D3D11 管线状态管理器
 */
class DX11PipelineStateManager {
public:
    DX11PipelineStateManager() = default;
    ~DX11PipelineStateManager() = default;

    /// 初始化
    void Init(DX11Context* context);

    /// 清理
    void Shutdown();

    /// 创建管线状态并返回句柄
    unsigned int CreatePipelineState(const PipelineStateDesc& desc);

    /// 查询管线状态
    const DX11PipelineState* GetPipelineState(unsigned int handle) const;

    /// 应用管线状态到设备上下文
    void ApplyPipelineState(unsigned int handle, ID3D11DeviceContext* dc);

    /// 设置/获取活跃管线状态
    void set_active_pipeline_state(unsigned int handle) { active_pipeline_state_ = handle; }
    unsigned int active_pipeline_state() const { return active_pipeline_state_; }

    /// 枚举转换
    static D3D11_BLEND ToD3D11Blend(BlendFactor factor);
    static D3D11_COMPARISON_FUNC ToD3D11ComparisonFunc(CompareFunc func);
    static D3D11_CULL_MODE ToD3D11CullMode(CullFace face);

private:
    DX11Context* context_ = nullptr;

    std::unordered_map<unsigned int, DX11PipelineState> pipeline_states_;
    unsigned int next_handle_ = 850000;
    unsigned int active_pipeline_state_ = 0;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_PIPELINE_STATE_MANAGER_H
