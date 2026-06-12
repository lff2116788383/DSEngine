/**
 * @file dx11_context.cpp
 * @brief D3D11 上下文实现 — Device/DeviceContext/SwapChain 生命周期管理
 */

#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/base/debug.h"

#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_2.h>
#include <dxgi1_5.h>
#include <chrono>
#include <cstdlib>

namespace dse {
namespace render {

namespace {
// 将宽字符（UTF-16）适配器名转为 UTF-8，避免逐字符截断丢失非 ASCII 字符
std::string WideToUtf8(const wchar_t* w) {
    if (!w || !*w) return {};
    int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 1) return {};
    std::string out(static_cast<size_t>(len - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, out.data(), len, nullptr, nullptr);
    return out;
}
} // namespace

bool DX11Context::Init(void* window_handle, int width, int height, bool enable_debug, bool force_sdr) {
    if (initialized_) return true;

    if (!CreateDeviceAndSwapChain(window_handle, width, height, enable_debug, force_sdr)) {
        DEBUG_LOG_ERROR("[D3D11] Failed to create device and swap chain");
        return false;
    }

    width_ = width;
    height_ = height;

    if (!CreateBackbufferViews()) {
        DEBUG_LOG_ERROR("[D3D11] Failed to create backbuffer views");
        return false;
    }
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
        UINT flags = (!vsync && tearing_supported_) ? DXGI_PRESENT_ALLOW_TEARING : 0;
        const bool diag = []() {
            const char* env = std::getenv("DSE_DX11_PRESENT_DIAG");
            return env && env[0] != '\0' && env[0] != '0';
        }();
        auto begin = std::chrono::high_resolution_clock::now();
        swapchain_->Present(vsync ? 1 : 0, flags);
        if (diag) {
            static int frame = 0;
            auto end = std::chrono::high_resolution_clock::now();
            const float ms = std::chrono::duration<float, std::milli>(end - begin).count();
            if (frame < 20 || (frame % 60) == 0) {
                DEBUG_LOG_INFO("[D3D11] Present frame={} ms={} vsync={} flags={}",
                    frame, ms, vsync ? 1 : 0, flags);
            }
            ++frame;
        }
    }
}

bool DX11Context::Resize(int width, int height) {
    if (!initialized_ || width <= 0 || height <= 0) return false;

    ReleaseBackbufferViews();

    UINT resize_flags = tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    HRESULT hr = swapchain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, resize_flags);
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] ResizeBuffers failed: HRESULT=0x{}", static_cast<unsigned>(hr));
        return false;
    }

    width_ = width;
    height_ = height;

    return CreateBackbufferViews();
}

bool DX11Context::CreateDeviceAndSwapChain(void* window_handle, int width, int height, bool enable_debug, bool force_sdr) {
    UINT create_flags = 0;
    if (enable_debug) {
        create_flags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    // DSE_FORCE_WARP=1：强制使用 WARP 软件光栅器（无 GPU 环境/调试用）
    const char* warp_env = std::getenv("DSE_FORCE_WARP");
    const bool force_warp = warp_env && warp_env[0] && warp_env[0] != '0';

    D3D_FEATURE_LEVEL feature_levels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
        D3D_FEATURE_LEVEL_10_1,
        D3D_FEATURE_LEVEL_10_0,
    };
    UINT num_feature_levels = ARRAYSIZE(feature_levels);

    // --- 枚举 DXGI 适配器，优先选择独立 GPU（跳过 software/virtual adapter）---
    ComPtr<IDXGIFactory1> dxgi_factory;
    IDXGIAdapter1* preferred_adapter = nullptr;
    SIZE_T best_vram = 0;
    if (!force_warp && SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(dxgi_factory.GetAddressOf())))) {
        ComPtr<IDXGIAdapter1> adapter;
        for (UINT i = 0; dxgi_factory->EnumAdapters1(i, adapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);
            // 跳过 software adapter (WARP / Basic Render)
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                DEBUG_LOG_INFO("[D3D11] Adapter {}: [SKIP software] {}", i,
                    WideToUtf8(desc.Description));
                continue;
            }
            // 跳过 dedicated VRAM = 0 的虚拟适配器（远程桌面虚拟显卡等）
            if (desc.DedicatedVideoMemory == 0) {
                DEBUG_LOG_INFO("[D3D11] Adapter {}: [SKIP no VRAM] {}", i,
                    WideToUtf8(desc.Description));
                continue;
            }
            DEBUG_LOG_INFO("[D3D11] Adapter {}: {} (VRAM={} MB)", i,
                WideToUtf8(desc.Description),
                desc.DedicatedVideoMemory / (1024 * 1024));
            if (desc.DedicatedVideoMemory > best_vram) {
                best_vram = desc.DedicatedVideoMemory;
                preferred_adapter = adapter.Detach();
            }
        }
    }
    // 使用显式适配器时 DriverType 必须为 D3D_DRIVER_TYPE_UNKNOWN；WARP 时必须 pAdapter=nullptr
    const D3D_DRIVER_TYPE driver_type =
        force_warp ? D3D_DRIVER_TYPE_WARP
        : (preferred_adapter ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE);
    is_warp_ = (driver_type == D3D_DRIVER_TYPE_WARP);
    if (preferred_adapter) {
        DXGI_ADAPTER_DESC1 desc{};
        static_cast<IDXGIAdapter1*>(preferred_adapter)->GetDesc1(&desc);
        DEBUG_LOG_INFO("[D3D11] Selected adapter: {} (VRAM={} MB)",
            WideToUtf8(desc.Description),
            desc.DedicatedVideoMemory / (1024 * 1024));
    }

    // HDR 尝试路径：R16G16B16A16_FLOAT + FLIP_DISCARD（Windows 10+）
    // force_sdr=true 时跳过 HDR 尝试，直接走 SDR 回退路径
    DXGI_SWAP_CHAIN_DESC scd{};
    scd.BufferCount = 2;
    scd.BufferDesc.Width = static_cast<UINT>(width);
    scd.BufferDesc.Height = static_cast<UINT>(height);
    scd.BufferDesc.Format = force_sdr ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R16G16B16A16_FLOAT;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = static_cast<HWND>(window_handle);
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = force_sdr ? DXGI_SWAP_EFFECT_DISCARD : DXGI_SWAP_EFFECT_FLIP_DISCARD;

    // DXGI_PRESENT_ALLOW_TEARING 支持检查（Windows 10 1607+）
    tearing_supported_ = false;
    if (!force_sdr) { // FLIP_DISCARD 才能用 ALLOW_TEARING
        ComPtr<IDXGIFactory5> factory5;
        if (dxgi_factory && SUCCEEDED(dxgi_factory.As(&factory5))) {
            BOOL allow_tearing = FALSE;
            if (SUCCEEDED(factory5->CheckFeatureSupport(
                    DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allow_tearing, sizeof(allow_tearing)))) {
                tearing_supported_ = (allow_tearing == TRUE);
            }
        }
    }
    scd.Flags = tearing_supported_ ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
    DEBUG_LOG_INFO("[D3D11] Tearing support: {}", tearing_supported_ ? "YES" : "NO");

    // 设备创建辅助：若因调试层缺失（缺少 Graphics Tools）失败，去掉 DEBUG 标志重试一次。
    // 用 ReleaseAndGetAddressOf 保证重试不泄漏上一次的 COM 引用。
    auto create_device = [&](IDXGIAdapter1* adapter, D3D_DRIVER_TYPE dtype) -> HRESULT {
        HRESULT h = D3D11CreateDeviceAndSwapChain(
            adapter, dtype, nullptr,
            create_flags, feature_levels, num_feature_levels,
            D3D11_SDK_VERSION, &scd,
            swapchain_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(),
            &feature_level_, context_.ReleaseAndGetAddressOf()
        );
        if (h == DXGI_ERROR_SDK_COMPONENT_MISSING && (create_flags & D3D11_CREATE_DEVICE_DEBUG)) {
            DEBUG_LOG_WARN("[D3D11] D3D11 debug layer unavailable; retrying without DEBUG flag");
            create_flags &= ~static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
            h = D3D11CreateDeviceAndSwapChain(
                adapter, dtype, nullptr,
                create_flags, feature_levels, num_feature_levels,
                D3D11_SDK_VERSION, &scd,
                swapchain_.ReleaseAndGetAddressOf(), device_.ReleaseAndGetAddressOf(),
                &feature_level_, context_.ReleaseAndGetAddressOf()
            );
        }
        return h;
    };

    HRESULT hr = create_device(preferred_adapter, driver_type);

    if (SUCCEEDED(hr)) {
        hdr_enabled_ = !force_sdr;
        if (force_sdr) {
            DEBUG_LOG_INFO("[D3D11] SDR SwapChain (R8G8B8A8_UNORM, forced via DSE_FORCE_SDR){}", is_warp_ ? " [WARP]" : "");
        } else {
            DEBUG_LOG_INFO("[D3D11] HDR SwapChain enabled (R16G16B16A16_FLOAT, FLIP_DISCARD){}", is_warp_ ? " [WARP]" : "");
        }
    } else {
        // SDR 回退：R8G8B8A8_UNORM + DISCARD（DISCARD 不支持 ALLOW_TEARING）
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        scd.Flags = 0;
        tearing_supported_ = false;
        hr = create_device(preferred_adapter, driver_type);
        if (FAILED(hr) && driver_type != D3D_DRIVER_TYPE_WARP) {
            // 硬件设备创建失败（无独显 / 无 GPU / 远程桌面会话）→ 回退到 WARP 软件光栅器，
            // 使无 GPU 环境（CI、服务器、远程桌面）也能初始化 D3D11
            DEBUG_LOG_WARN("[D3D11] Hardware device creation failed (HRESULT=0x{}), falling back to WARP software rasterizer",
                static_cast<unsigned>(hr));
            hr = create_device(nullptr, D3D_DRIVER_TYPE_WARP);
            if (SUCCEEDED(hr)) is_warp_ = true;
        }
        if (FAILED(hr)) {
            DEBUG_LOG_ERROR("[D3D11] D3D11CreateDeviceAndSwapChain failed (incl. WARP fallback): HRESULT=0x{}", static_cast<unsigned>(hr));
            if (preferred_adapter) preferred_adapter->Release();
            return false;
        }
        hdr_enabled_ = false;
        DEBUG_LOG_INFO("[D3D11] SDR SwapChain (R8G8B8A8_UNORM, DISCARD){}", is_warp_ ? " [WARP]" : "");
    }
    if (preferred_adapter) preferred_adapter->Release();

    // 回查实际所选适配器名称 + 软件渲染判定：覆盖「HARDWARE 驱动类型落到
    // Microsoft Basic Render Driver」这种 is_warp_ 漏标的软渲情况，供性能基准
    // 标注后端实际跑在硬件还是软渲（避免把软渲数当硬件数）。
    {
        ComPtr<IDXGIDevice> dxgi_device;
        ComPtr<IDXGIAdapter> dxgi_adapter;
        if (SUCCEEDED(device_.As(&dxgi_device)) &&
            SUCCEEDED(dxgi_device->GetAdapter(dxgi_adapter.GetAddressOf()))) {
            ComPtr<IDXGIAdapter1> dxgi_adapter1;
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(dxgi_adapter.As(&dxgi_adapter1)) &&
                SUCCEEDED(dxgi_adapter1->GetDesc1(&desc))) {
                adapter_name_ = WideToUtf8(desc.Description);
                is_software_ = is_warp_
                    || (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0
                    || adapter_name_.find("Basic Render") != std::string::npos;
            }
        }
        DEBUG_LOG_INFO("[D3D11] Render device: {} ({})", adapter_name_,
            is_software_ ? "software" : "hardware");
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
