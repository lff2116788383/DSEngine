/**
 * @file dx11_context.cpp
 * @brief D3D11 上下文实现 — Device/DeviceContext/SwapChain 生命周期管理
 */

#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/base/debug.h"

#include <d3d11.h>
#include <dxgi.h>

namespace dse {
namespace render {

bool DX11Context::Init(void* window_handle, int width, int height, bool enable_debug) {
    if (initialized_) return true;

    if (!CreateDeviceAndSwapChain(window_handle, width, height, enable_debug)) {
        DEBUG_LOG_ERROR("[D3D11] Failed to create device and swap chain");
        return false;
    }

    if (!CreateBackbufferViews()) {
        DEBUG_LOG_ERROR("[D3D11] Failed to create backbuffer views");
        return false;
    }

    width_ = width;
    height_ = height;
    initialized_ = true;

    DEBUG_LOG_INFO("[D3D11] Context initialized — Feature Level: {}", FeatureLevelString());
    return true;
}

void DX11Context::Shutdown() {
    if (!initialized_) return;

    // 确保 GPU 完成所有工作
    if (context_) {
        context_->ClearState();
        context_->Flush();
    }

    ReleaseBackbufferViews();
    swapchain_.Reset();
    context_.Reset();
    device_.Reset();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] Context shutdown");
}

void DX11Context::Present(bool vsync) {
    if (swapchain_) {
        swapchain_->Present(vsync ? 1 : 0, 0);
    }
}

bool DX11Context::Resize(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return false;

    ReleaseBackbufferViews();

    HRESULT hr = swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] ResizeBuffers failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    width_ = width;
    height_ = height;

    return CreateBackbufferViews();
}

bool DX11Context::CreateDeviceAndSwapChain(void* window_handle, int width, int height, bool enable_debug) {
    UINT create_flags = 0;
    if (enable_debug) {
        create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT num_feature_levels = ARRAYSIZE(feature_levels);

    // HDR 尝试路径：R16G16B16A16_FLOAT + FLIP_DISCARD（Windows 10+）
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = static_cast<UINT>(width);
    scd.BufferDesc.Height = static_cast<UINT>(height);
    scd.BufferDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = static_cast<HWND>(window_handle);
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scd.Flags = 0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        create_flags, feature_levels, num_feature_levels,
        D3D11_SDK_VERSION, &scd,
        swapchain_.GetAddressOf(), device_.GetAddressOf(),
        &feature_level_, context_.GetAddressOf()
    );

    if (SUCCEEDED(hr)) {
        hdr_enabled_ = true;
        DEBUG_LOG_INFO("[D3D11] HDR SwapChain enabled (R16G16B16A16_FLOAT, FLIP_DISCARD)");
    } else {
        // SDR 回退：R8G8B8A8_UNORM + DISCARD
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        hr = D3D11CreateDeviceAndSwapChain(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            create_flags, feature_levels, num_feature_levels,
            D3D11_SDK_VERSION, &scd,
            swapchain_.GetAddressOf(), device_.GetAddressOf(),
            &feature_level_, context_.GetAddressOf()
        );
        if (FAILED(hr)) {
            DEBUG_LOG_ERROR("[D3D11] D3D11CreateDeviceAndSwapChain failed: 0x{:08X}", static_cast<unsigned>(hr));
            return false;
        }
        hdr_enabled_ = false;
        DEBUG_LOG_INFO("[D3D11] SDR SwapChain (R8G8B8A8_UNORM, DISCARD)");
    }

    // MSAA 4x 支持查询（在设备创建后）
    UINT quality = 0;
    hr = device_->CheckMultisampleQualityLevels(DXGI_FORMAT_R16G16B16A16_FLOAT, 4, &quality);
    msaa_4x_quality_ = (SUCCEEDED(hr) && quality > 0) ? quality - 1 : 0;
    DEBUG_LOG_INFO("[D3D11] MSAA 4x quality={}", msaa_4x_quality_);
    if (msaa_4x_quality_ == 0) {
        DEBUG_LOG_INFO("[D3D11] MSAA 4x not supported, will use 1x");
    }

    return true;
}

bool DX11Context::CreateBackbufferViews() {
    // 获取后备缓冲区并创建 RTV
    ComPtr<ID3D11Texture2D> backbuffer;
    HRESULT hr = swapchain_->GetBuffer(0, IID_PPV_ARGS(backbuffer.GetAddressOf()));
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] GetBuffer failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    hr = device_->CreateRenderTargetView(backbuffer.Get(), nullptr, backbuffer_rtv_.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateRenderTargetView failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    // 创建深度/模板缓冲区
    D3D11_TEXTURE2D_DESC depth_desc{};
    depth_desc.Width = static_cast<UINT>(width_);
    depth_desc.Height = static_cast<UINT>(height_);
    depth_desc.MipLevels = 1;
    depth_desc.ArraySize = 1;
    depth_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depth_desc.SampleDesc.Count = 1;
    depth_desc.SampleDesc.Quality = 0;
    depth_desc.Usage = D3D11_USAGE_DEFAULT;
    depth_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    depth_desc.CPUAccessFlags = 0;
    depth_desc.MiscFlags = 0;

    hr = device_->CreateTexture2D(&depth_desc, nullptr, depth_stencil_texture_.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateTexture2D (depth) failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    hr = device_->CreateDepthStencilView(depth_stencil_texture_.Get(), nullptr, backbuffer_dsv_.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateDepthStencilView failed: 0x{:08X}", static_cast<unsigned>(hr));
        return false;
    }

    return true;
}

void DX11Context::ReleaseBackbufferViews() {
    if (context_) {
        context_->OMSetRenderTargets(0, nullptr, nullptr);
    }
    backbuffer_rtv_.Reset();
    backbuffer_dsv_.Reset();
    depth_stencil_texture_.Reset();
}

std::string DX11Context::FeatureLevelString() const {
    switch (feature_level_) {
        case D3D_FEATURE_LEVEL_11_1: return "11.1";
        case D3D_FEATURE_LEVEL_11_0: return "11.0";
        case D3D_FEATURE_LEVEL_10_1: return "10.1";
        case D3D_FEATURE_LEVEL_10_0: return "10.0";
        default: return "Unknown";
    }
}

} // namespace render
} // namespace dse
