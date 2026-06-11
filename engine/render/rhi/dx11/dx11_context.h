/**
 * @file dx11_context.h
 * @brief D3D11 上下文 — 管理 ID3D11Device、ID3D11DeviceContext 和 IDXGISwapChain
 *
 * 职责：
 * 1. D3D11 设备与即时上下文创建
 * 2. Feature Level 检测
 * 3. SwapChain 创建与 Present
 * 4. 后备缓冲区 RTV 与 DSV 管理
 */

#ifndef DSE_RENDER_DX11_CONTEXT_H
#define DSE_RENDER_DX11_CONTEXT_H

#include <d3d11.h>
#include <dxgi.h>
#include <wrl/client.h>
#include <string>

namespace dse {
namespace render {

using Microsoft::WRL::ComPtr;

/**
 * @class DX11Context
 * @brief D3D11 上下文，持有 Device/DeviceContext/SwapChain 的完整生命周期
 *
 * 使用模式：
 *   DX11Context ctx;
 *   ctx.Init(hwnd, width, height);
 *   // ... 渲染循环 ...
 *   ctx.Shutdown();
 */
class DX11Context {
public:
    DX11Context() = default;
    ~DX11Context() = default;

    // 禁止拷贝
    DX11Context(const DX11Context&) = delete;
    DX11Context& operator=(const DX11Context&) = delete;

    /// 初始化 D3D11 Device/DeviceContext/SwapChain
    /// @param window_handle Win32 HWND
    /// @param width 窗口宽度
    /// @param height 窗口高度
    /// @param enable_debug 是否启用 D3D11 调试层
    /// @param force_sdr 强制使用 SDR (R8G8B8A8) 交换链，跳过 HDR 尝试
    bool Init(void* window_handle, int width, int height, bool enable_debug = false, bool force_sdr = false);

    /// 关闭并释放所有 D3D11 资源
    void Shutdown();

    /// Present 交换链
    void Present(bool vsync = true);

    /// 窗口大小变化时重建后备缓冲区
    bool Resize(int width, int height);

    // --- 访问器 ---
    ID3D11Device* device() const { return device_.Get(); }
    ID3D11DeviceContext* context() const { return context_.Get(); }
    ID3D11DeviceContext* device_context() const { return context_.Get(); }
    IDXGISwapChain* swapchain() const { return swapchain_.Get(); }

    ID3D11RenderTargetView* backbuffer_rtv() const { return backbuffer_rtv_.Get(); }
    ID3D11DepthStencilView* backbuffer_dsv() const { return backbuffer_dsv_.Get(); }

    D3D_FEATURE_LEVEL feature_level() const { return feature_level_; }
    int width() const { return width_; }
    int height() const { return height_; }
    bool initialized() const { return initialized_; }
    bool hdr_enabled() const { return hdr_enabled_; }
    bool tearing_supported() const { return tearing_supported_; }
    UINT msaa_4x_quality() const { return msaa_4x_quality_; }
    bool is_warp() const { return is_warp_; }

    /// Feature Level 转可读字符串
    std::string FeatureLevelString() const;

private:
    bool CreateDeviceAndSwapChain(void* window_handle, int width, int height, bool enable_debug, bool force_sdr);
    bool CreateBackbufferViews();
    void ReleaseBackbufferViews();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGISwapChain> swapchain_;

    ComPtr<ID3D11RenderTargetView> backbuffer_rtv_;
    ComPtr<ID3D11DepthStencilView> backbuffer_dsv_;
    ComPtr<ID3D11Texture2D> depth_stencil_texture_;

    D3D_FEATURE_LEVEL feature_level_ = D3D_FEATURE_LEVEL_11_0;
    int width_ = 0;
    int height_ = 0;
    bool initialized_ = false;
    bool hdr_enabled_ = false;     ///< SwapChain 是否使用 R16G16B16A16_FLOAT
    bool tearing_supported_ = false; ///< DXGI_PRESENT_ALLOW_TEARING 是否可用
    UINT msaa_4x_quality_ = 0;    ///< MSAA 4x 质量等级（0 = 不支持）
    bool is_warp_ = false;        ///< 是否运行在 WARP 软件光栅器（无 GPU 回退）
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_CONTEXT_H
