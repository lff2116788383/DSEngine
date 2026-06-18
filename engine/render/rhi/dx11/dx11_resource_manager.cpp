/**
 * @file dx11_resource_manager.cpp
 * @brief DX11ResourceManager 实现 — D3D11 GPU 资源的创建/销毁/更新
 */

#include "engine/render/rhi/dx11/dx11_resource_manager.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/base/debug.h"

#include <cstring>
#include <algorithm>
#include <cmath>

namespace dse {
namespace render {

static float HalfToFloat(uint16_t h) {
    uint32_t sign = static_cast<uint32_t>(h >> 15) << 31;
    uint32_t exponent = (h >> 10) & 0x1Fu;
    uint32_t mantissa = h & 0x3FFu;
    uint32_t f;
    if (exponent == 0) {
        if (mantissa == 0) {
            f = sign;
        } else {
            exponent = 1;
            while (!(mantissa & 0x400u)) { mantissa <<= 1; exponent--; }
            mantissa &= 0x3FFu;
            f = sign | ((exponent + 112u) << 23) | (mantissa << 13);
        }
    } else if (exponent == 31) {
        f = sign | 0x7F800000u | (mantissa << 13);
    } else {
        f = sign | ((exponent + 112u) << 23) | (mantissa << 13);
    }
    float result;
    std::memcpy(&result, &f, 4);
    return result;
}

bool DX11ResourceManager::Init(DX11Context* context) {
    context_ = context;
    device_ = context->device();
    dc_ = context->device_context();
    initialized_ = true;
    DEBUG_LOG_INFO("[D3D11] ResourceManager initialized");
    return true;
}

void DX11ResourceManager::Shutdown() {
    if (!initialized_) return;

    textures_.clear();
    buffers_.clear();
    ssbos_.clear();
    render_targets_.clear();
    vertex_arrays_.clear();
    indirect_buffers_.clear();

    initialized_ = false;
    DEBUG_LOG_INFO("[D3D11] ResourceManager shutdown");
}

// ============================================================
// 纹理
// ============================================================

unsigned int DX11ResourceManager::CreateTexture2D(int width, int height,
                                                    const unsigned char* rgba8_data,
                                                    bool linear_filter) {
    if (!device_) return 0;
    DX11Texture tex;
    tex.width = width;
    tex.height = height;
    tex.is_cube = false;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = rgba8_data;
    init_data.SysMemPitch = static_cast<UINT>(width * 4);

    HRESULT hr = device_->CreateTexture2D(&td, rgba8_data ? &init_data : nullptr, tex.texture.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateTexture2D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    // SRV
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = td.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    hr = device_->CreateShaderResourceView(tex.texture.Get(), &srv_desc, tex.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    // Sampler
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = linear_filter ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sd, tex.sampler.GetAddressOf());
    if (FAILED(hr)) return 0;

    unsigned int handle = next_texture_handle_++;
    textures_[handle] = std::move(tex);
    return handle;
}

unsigned int DX11ResourceManager::CreateComputeWriteTexture2D(int width, int height) {
    if (!device_) return 0;
    DX11Texture tex;
    tex.width = width;
    tex.height = height;

    D3D11_TEXTURE2D_DESC td{};
    td.Width     = static_cast<UINT>(width);
    td.Height    = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format    = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage     = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    HRESULT hr = device_->CreateTexture2D(&td, nullptr, tex.texture.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateComputeWriteTexture2D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = td.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;
    hr = device_->CreateShaderResourceView(tex.texture.Get(), &srv_desc, tex.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format        = td.Format;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
    hr = device_->CreateUnorderedAccessView(tex.texture.Get(), &uav_desc, tex.uav.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter   = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    device_->CreateSamplerState(&sd, tex.sampler.GetAddressOf());

    unsigned int handle = next_texture_handle_++;
    textures_[handle] = std::move(tex);
    DEBUG_LOG_INFO("[D3D11] Compute write texture created: handle={} {}x{}", handle, width, height);
    return handle;
}

unsigned int DX11ResourceManager::CreateCompressedTexture2D(CompressedTextureFormat format,
                                                              const std::vector<CompressedMipLevel>& mips,
                                                              bool linear_filter) {
    if (!device_ || mips.empty()) return 0;

    DXGI_FORMAT dxgi_fmt = DXGI_FORMAT_UNKNOWN;
    switch (format) {
        case CompressedTextureFormat::BC1_UNORM: dxgi_fmt = DXGI_FORMAT_BC1_UNORM; break;
        case CompressedTextureFormat::BC1_SRGB:  dxgi_fmt = DXGI_FORMAT_BC1_UNORM_SRGB; break;
        case CompressedTextureFormat::BC2_UNORM: dxgi_fmt = DXGI_FORMAT_BC2_UNORM; break;
        case CompressedTextureFormat::BC3_UNORM: dxgi_fmt = DXGI_FORMAT_BC3_UNORM; break;
        case CompressedTextureFormat::BC3_SRGB:  dxgi_fmt = DXGI_FORMAT_BC3_UNORM_SRGB; break;
        case CompressedTextureFormat::BC4_UNORM: dxgi_fmt = DXGI_FORMAT_BC4_UNORM; break;
        case CompressedTextureFormat::BC5_UNORM: dxgi_fmt = DXGI_FORMAT_BC5_UNORM; break;
        case CompressedTextureFormat::BC7_UNORM: dxgi_fmt = DXGI_FORMAT_BC7_UNORM; break;
        case CompressedTextureFormat::BC7_SRGB:  dxgi_fmt = DXGI_FORMAT_BC7_UNORM_SRGB; break;
        default: return 0;
    }

    UINT block_size = 4;
    auto row_pitch = [&](int w) -> UINT {
        UINT blocks_wide = std::max<UINT>(1u, (static_cast<UINT>(w) + block_size - 1) / block_size);
        UINT bytes_per_block = (format == CompressedTextureFormat::BC1_UNORM ||
                                format == CompressedTextureFormat::BC1_SRGB ||
                                format == CompressedTextureFormat::BC4_UNORM) ? 8u : 16u;
        return blocks_wide * bytes_per_block;
    };

    DX11Texture tex;
    tex.width = mips[0].width;
    tex.height = mips[0].height;
    tex.is_cube = false;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(mips[0].width);
    td.Height = static_cast<UINT>(mips[0].height);
    td.MipLevels = static_cast<UINT>(mips.size());
    td.ArraySize = 1;
    td.Format = dxgi_fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    std::vector<D3D11_SUBRESOURCE_DATA> init_data(mips.size());
    for (size_t i = 0; i < mips.size(); ++i) {
        init_data[i].pSysMem = mips[i].data;
        init_data[i].SysMemPitch = row_pitch(mips[i].width);
        init_data[i].SysMemSlicePitch = 0;
    }

    HRESULT hr = device_->CreateTexture2D(&td, init_data.data(), tex.texture.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateCompressedTexture2D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = dxgi_fmt;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = td.MipLevels;
    hr = device_->CreateShaderResourceView(tex.texture.Get(), &srv_desc, tex.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = linear_filter ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sd, tex.sampler.GetAddressOf());
    if (FAILED(hr)) return 0;

    unsigned int handle = next_texture_handle_++;
    textures_[handle] = std::move(tex);
    return handle;
}

unsigned int DX11ResourceManager::CreateTextureCube(int width, int height,
                                                      const unsigned char* const rgba8_faces[6],
                                                      bool linear_filter) {
    DX11Texture tex;
    tex.width = width;
    tex.height = height;
    tex.is_cube = true;

    D3D11_TEXTURE2D_DESC td{};
    td.Width = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 6;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_SUBRESOURCE_DATA init_data[6]{};
    bool has_data = (rgba8_faces != nullptr);
    if (has_data) {
        for (int i = 0; i < 6; ++i) {
            init_data[i].pSysMem = rgba8_faces[i];
            init_data[i].SysMemPitch = static_cast<UINT>(width * 4);
        }
    }

    HRESULT hr = device_->CreateTexture2D(&td, has_data ? init_data : nullptr, tex.texture.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = td.Format;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srv_desc.TextureCube.MipLevels = 1;
    hr = device_->CreateShaderResourceView(tex.texture.Get(), &srv_desc, tex.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter = linear_filter ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sd, tex.sampler.GetAddressOf());
    if (FAILED(hr)) return 0;

    unsigned int handle = next_texture_handle_++;
    textures_[handle] = std::move(tex);
    return handle;
}

unsigned int DX11ResourceManager::CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) {
    if (!device_ || width <= 0 || height <= 0 || depth <= 0) return 0;

    D3D11_TEXTURE3D_DESC td{};
    td.Width     = static_cast<UINT>(width);
    td.Height    = static_cast<UINT>(height);
    td.Depth     = static_cast<UINT>(depth);
    td.MipLevels = 1;
    td.Format    = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.Usage     = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem          = rgba8_data;
    init_data.SysMemPitch      = static_cast<UINT>(width * 4);
    init_data.SysMemSlicePitch = static_cast<UINT>(width * height * 4);

    ComPtr<ID3D11Texture3D> tex3d;
    HRESULT hr = device_->CreateTexture3D(&td, rgba8_data ? &init_data : nullptr, tex3d.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateTexture3D failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    DX11Texture tex;
    tex.width  = width;
    tex.height = height;
    tex.depth  = depth;
    tex.is_3d  = true;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format                    = td.Format;
    srv_desc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE3D;
    srv_desc.Texture3D.MipLevels       = 1;
    srv_desc.Texture3D.MostDetailedMip = 0;
    hr = device_->CreateShaderResourceView(tex3d.Get(), &srv_desc, tex.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_SAMPLER_DESC sd{};
    sd.Filter   = linear_filter ? D3D11_FILTER_MIN_MAG_MIP_LINEAR : D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy  = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    hr = device_->CreateSamplerState(&sd, tex.sampler.GetAddressOf());
    if (FAILED(hr)) return 0;

    unsigned int handle = next_texture_handle_++;
    textures_[handle] = std::move(tex);
    return handle;
}

void DX11ResourceManager::DeleteTexture(unsigned int handle) {
    textures_.erase(handle);
}

const DX11Texture* DX11ResourceManager::GetTexture(unsigned int handle) const {
    auto it = textures_.find(handle);
    return it != textures_.end() ? &it->second : nullptr;
}

// ============================================================
// 缓冲区
// ============================================================

unsigned int DX11ResourceManager::CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) {
    if (!device_) return 0;
    DX11Buffer buf;
    buf.size = size;
    buf.is_dynamic = is_dynamic;
    buf.is_index = is_index;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(size);
    bd.BindFlags = is_index ? D3D11_BIND_INDEX_BUFFER : D3D11_BIND_VERTEX_BUFFER;

    if (is_dynamic) {
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else {
        bd.Usage = D3D11_USAGE_DEFAULT;
    }

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = data;

    HRESULT hr = device_->CreateBuffer(&bd, data ? &init_data : nullptr, buf.buffer.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateBuffer failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    unsigned int handle = next_buffer_handle_++;
    buffers_[handle] = std::move(buf);
    return handle;
}

unsigned int DX11ResourceManager::CreateConstantBuffer(size_t size, const void* data, bool is_dynamic) {
    if (!device_) return 0;
    DX11Buffer buf;
    buf.size = size;
    buf.is_dynamic = is_dynamic;
    buf.is_index = false;

    D3D11_BUFFER_DESC bd{};
    // constant buffer 的 ByteWidth 必须是 16 的倍数
    bd.ByteWidth = static_cast<UINT>((size + 15) & ~size_t(15));
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    if (is_dynamic) {
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    } else {
        bd.Usage = D3D11_USAGE_DEFAULT;
    }

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = data;

    HRESULT hr = device_->CreateBuffer(&bd, data ? &init_data : nullptr, buf.buffer.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateConstantBuffer failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    unsigned int handle = next_buffer_handle_++;
    buffers_[handle] = std::move(buf);
    return handle;
}

void DX11ResourceManager::UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool /*is_index*/) {
    if (!device_) return;
    auto it = buffers_.find(handle);
    if (it == buffers_.end()) return;
    auto& buf = it->second;

    if (buf.is_dynamic) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        HRESULT hr = dc_->Map(buf.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
        if (SUCCEEDED(hr)) {
            memcpy(static_cast<unsigned char*>(mapped.pData) + offset, data, size);
            dc_->Unmap(buf.buffer.Get(), 0);
        }
    } else {
        D3D11_BOX box{};
        box.left = static_cast<UINT>(offset);
        box.right = static_cast<UINT>(offset + size);
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        dc_->UpdateSubresource(buf.buffer.Get(), 0, &box, data, 0, 0);
    }
}

void DX11ResourceManager::DeleteBuffer(unsigned int handle) {
    buffers_.erase(handle);
}

const DX11Buffer* DX11ResourceManager::GetBuffer(unsigned int handle) const {
    auto it = buffers_.find(handle);
    return it != buffers_.end() ? &it->second : nullptr;
}

// ============================================================
// SSBO (ByteAddressBuffer + Raw SRV)
// ============================================================

unsigned int DX11ResourceManager::CreateSSBO(size_t size, const void* data) {
    unsigned int handle = next_ssbo_handle_++;
    DX11SSBO ssbo;
    // ByteAddressBuffer 要求 4 字节对齐
    ssbo.size = (size + 3) & ~3;
    ssbo.stride = 4; // raw buffer element size

    D3D11_BUFFER_DESC desc{};
    desc.ByteWidth = static_cast<UINT>(ssbo.size);
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
                   | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = data;

    HRESULT hr = device_->CreateBuffer(&desc, data ? &init_data : nullptr, ssbo.buffer.GetAddressOf());
    if (FAILED(hr)) return 0;

    // SRV (ByteAddressBuffer readonly 访问)
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srv_desc.BufferEx.FirstElement = 0;
    srv_desc.BufferEx.NumElements = static_cast<UINT>(ssbo.size / 4);
    srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

    hr = device_->CreateShaderResourceView(ssbo.buffer.Get(), &srv_desc, ssbo.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    // UAV (RWByteAddressBuffer writable 访问)
    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
    uav_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    uav_desc.Buffer.FirstElement = 0;
    uav_desc.Buffer.NumElements = static_cast<UINT>(ssbo.size / 4);
    uav_desc.Buffer.Flags = D3D11_BUFFER_UAV_FLAG_RAW;

    hr = device_->CreateUnorderedAccessView(ssbo.buffer.Get(), &uav_desc, ssbo.uav.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[DX11] SSBO UAV creation failed: hr=0x{:08X}", static_cast<unsigned>(hr));
        // 不致命 — 只读 SSBO 不需要 UAV
    }

    ssbos_[handle] = std::move(ssbo);
    return handle;
}

void DX11ResourceManager::UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) {
    auto it = ssbos_.find(handle);
    if (it == ssbos_.end() || !data || size == 0) return;

    if (offset == 0 && size == it->second.size) {
        dc_->UpdateSubresource(it->second.buffer.Get(), 0, nullptr, data, 0, 0);
    } else {
        D3D11_BOX dst_box{};
        dst_box.left   = static_cast<UINT>(offset);
        dst_box.right  = static_cast<UINT>(offset + size);
        dst_box.top    = 0; dst_box.bottom = 1;
        dst_box.front  = 0; dst_box.back   = 1;
        dc_->UpdateSubresource(it->second.buffer.Get(), 0, &dst_box, data, 0, 0);
    }
}

void DX11ResourceManager::BindSSBO(unsigned int handle, unsigned int binding_point) {
    auto it = ssbos_.find(handle);
    if (it == ssbos_.end()) return;
    // 绑定到 PS t-register（ssbo_register_base_ 由 reflection 计算，避免与纹理冲突）
    ID3D11ShaderResourceView* srv = it->second.srv.Get();
    dc_->PSSetShaderResources(ssbo_register_base_ + binding_point, 1, &srv);
}

void DX11ResourceManager::BindSSBOForCompute(unsigned int handle, unsigned int binding_point, bool writable) {
    auto it = ssbos_.find(handle);
    if (it == ssbos_.end()) return;

    if (writable) {
        // 绑定 UAV 到 CS u-register
        ID3D11UnorderedAccessView* uav = it->second.uav.Get();
        if (uav) {
            UINT initial_count = static_cast<UINT>(-1);
            dc_->CSSetUnorderedAccessViews(binding_point, 1, &uav, &initial_count);
        }
    } else {
        // 绑定 SRV 到 CS t-register（ssbo_register_base_ + binding）
        ID3D11ShaderResourceView* srv = it->second.srv.Get();
        dc_->CSSetShaderResources(ssbo_register_base_ + binding_point, 1, &srv);
    }
}

void DX11ResourceManager::DeleteSSBO(unsigned int handle) {
    ssbos_.erase(handle);
}

const DX11SSBO* DX11ResourceManager::GetSSBO(unsigned int handle) const {
    auto it = ssbos_.find(handle);
    return it != ssbos_.end() ? &it->second : nullptr;
}

ID3D11ShaderResourceView* DX11ResourceManager::GetSSBORangeSRV(unsigned int handle,
                                                              uint32_t offset, uint32_t size) {
    auto it = ssbos_.find(handle);
    if (it == ssbos_.end()) return nullptr;
    DX11SSBO& ssbo = it->second;
    if (offset == 0 && size == 0) return ssbo.srv.Get();  // 整块绑定

    // RAW BufferEx 按 4 字节元素寻址；子区间 SRV 按 (offset,size) 缓存。
    const uint64_t key = (static_cast<uint64_t>(offset) << 32) | static_cast<uint64_t>(size);
    auto cached = ssbo.range_srvs.find(key);
    if (cached != ssbo.range_srvs.end()) return cached->second.Get();

    const uint32_t range_bytes = (size != 0) ? size : (static_cast<uint32_t>(ssbo.size) - offset);
    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
    srv_desc.BufferEx.FirstElement = offset / 4;
    srv_desc.BufferEx.NumElements = range_bytes / 4;
    srv_desc.BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = device_->CreateShaderResourceView(ssbo.buffer.Get(), &srv_desc, srv.GetAddressOf());
    if (FAILED(hr)) return ssbo.srv.Get();  // 退回整块，避免空绑定
    ID3D11ShaderResourceView* raw = srv.Get();
    ssbo.range_srvs.emplace(key, std::move(srv));
    return raw;
}

// ============================================================
// 渲染目标
// ============================================================

unsigned int DX11ResourceManager::CreateRenderTarget(int width, int height, bool has_color, bool has_depth,
                                                       bool generate_mipmaps, bool /*cube_map*/,
                                                       int msaa_samples, bool allow_uav,
                                                       int color_attachment_count) {
    if (!device_) return 0;

    const int num_color = has_color ? (std::max)(1, color_attachment_count) : 0;

    // MSAA 有效性检查；MRT (>1 attachment) 禁用 MSAA
    const bool use_hdr = context_ ? context_->hdr_enabled() : false;
    const DXGI_FORMAT color_fmt = use_hdr ? DXGI_FORMAT_R16G16B16A16_FLOAT : DXGI_FORMAT_R8G8B8A8_UNORM;
    const UINT msaa_quality = (context_ && msaa_samples > 1) ? context_->msaa_4x_quality() : 0;
    const bool use_msaa = (msaa_samples > 1) && (msaa_quality > 0) && (num_color <= 1);
    const UINT actual_samples = use_msaa ? static_cast<UINT>(msaa_samples) : 1;

    DX11RenderTarget rt;
    rt.width = width;
    rt.height = height;
    rt.has_color = has_color;
    rt.has_depth = has_depth;
    rt.generate_mipmaps = generate_mipmaps;
    rt.is_msaa = use_msaa;
    rt.msaa_samples = use_msaa ? msaa_samples : 1;
    rt.color_attachment_count = num_color;

    // ---- 颜色附件 ----
    if (has_color) {
        // 1. 颜色纹理（MSAA 时为多重采样纹理，只绑定 RTV）
        D3D11_TEXTURE2D_DESC td{};
        td.Width  = static_cast<UINT>(width);
        td.Height = static_cast<UINT>(height);
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = color_fmt;
        td.SampleDesc.Count   = actual_samples;
        td.SampleDesc.Quality = use_msaa ? msaa_quality : 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET;
        if (!use_msaa) {
            td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
            if (allow_uav) td.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
        }

        if (num_color > 1 && !use_msaa) {
            // MRT 路径：创建 N 个独立颜色纹理
            rt.color_textures_mrt.resize(static_cast<size_t>(num_color));
            rt.color_rtvs_mrt.resize(static_cast<size_t>(num_color));
            rt.color_srvs_mrt.resize(static_cast<size_t>(num_color));
            rt.color_texture_handles_mrt.resize(static_cast<size_t>(num_color));

            for (int ci = 0; ci < num_color; ++ci) {
                HRESULT hr = device_->CreateTexture2D(&td, nullptr, rt.color_textures_mrt[ci].GetAddressOf());
                if (FAILED(hr)) {
                    DEBUG_LOG_ERROR("[D3D11] CreateTexture2D (MRT color {}) failed: 0x{:08X}", ci, static_cast<unsigned>(hr));
                    return 0;
                }
                hr = device_->CreateRenderTargetView(rt.color_textures_mrt[ci].Get(), nullptr, rt.color_rtvs_mrt[ci].GetAddressOf());
                if (FAILED(hr)) return 0;

                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Format = color_fmt;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = 1;
                hr = device_->CreateShaderResourceView(rt.color_textures_mrt[ci].Get(), &srv_desc,
                                                        rt.color_srvs_mrt[ci].GetAddressOf());
                if (FAILED(hr)) return 0;

                DX11Texture tex;
                tex.texture = rt.color_textures_mrt[ci];
                tex.srv     = rt.color_srvs_mrt[ci];
                tex.width   = width;
                tex.height  = height;
                rt.color_texture_handles_mrt[ci] = next_texture_handle_++;
                textures_[rt.color_texture_handles_mrt[ci]] = std::move(tex);
            }
            // 兼容：第一个纹理也赋给 color_texture/rtv/srv
            rt.color_texture = rt.color_textures_mrt[0];
            rt.color_rtv = rt.color_rtvs_mrt[0];
            rt.color_srv = rt.color_srvs_mrt[0];
            rt.color_texture_handle = rt.color_texture_handles_mrt[0];
        } else {
            // 单附件路径（含 MSAA）
            HRESULT hr = device_->CreateTexture2D(&td, nullptr, rt.color_texture.GetAddressOf());
            if (FAILED(hr)) {
                DEBUG_LOG_ERROR("[D3D11] CreateTexture2D (color) failed: 0x{:08X}", static_cast<unsigned>(hr));
                return 0;
            }

            hr = device_->CreateRenderTargetView(rt.color_texture.Get(), nullptr, rt.color_rtv.GetAddressOf());
            if (FAILED(hr)) return 0;

            if (use_msaa) {
                D3D11_TEXTURE2D_DESC resolve_td = td;
                resolve_td.SampleDesc.Count   = 1;
                resolve_td.SampleDesc.Quality = 0;
                resolve_td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
                if (allow_uav) resolve_td.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;

                hr = device_->CreateTexture2D(&resolve_td, nullptr, rt.color_resolve_texture.GetAddressOf());
                if (FAILED(hr)) return 0;

                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Format = color_fmt;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = 1;
                hr = device_->CreateShaderResourceView(rt.color_resolve_texture.Get(), &srv_desc,
                                                        rt.color_srv.GetAddressOf());
                if (FAILED(hr)) return 0;

                if (allow_uav) {
                    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
                    uav_desc.Format = color_fmt;
                    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                    device_->CreateUnorderedAccessView(rt.color_resolve_texture.Get(), &uav_desc,
                                                        rt.color_uav.GetAddressOf());
                }

                DX11Texture color_tex;
                color_tex.texture = rt.color_resolve_texture;
                color_tex.srv     = rt.color_srv;
                color_tex.width   = width;
                color_tex.height  = height;
                rt.color_texture_handle = next_texture_handle_++;
                textures_[rt.color_texture_handle] = std::move(color_tex);
            } else {
                D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
                srv_desc.Format = color_fmt;
                srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srv_desc.Texture2D.MipLevels = 1;
                hr = device_->CreateShaderResourceView(rt.color_texture.Get(), &srv_desc,
                                                        rt.color_srv.GetAddressOf());
                if (FAILED(hr)) return 0;

                if (allow_uav) {
                    D3D11_UNORDERED_ACCESS_VIEW_DESC uav_desc{};
                    uav_desc.Format = color_fmt;
                    uav_desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
                    device_->CreateUnorderedAccessView(rt.color_texture.Get(), &uav_desc,
                                                        rt.color_uav.GetAddressOf());
                }

                DX11Texture color_tex;
                color_tex.texture = rt.color_texture;
                color_tex.srv     = rt.color_srv;
                color_tex.width   = width;
                color_tex.height  = height;
                rt.color_texture_handle = next_texture_handle_++;
                textures_[rt.color_texture_handle] = std::move(color_tex);
            }
        }
    }

    // ---- 深度附件 ----
    if (has_depth) {
        D3D11_TEXTURE2D_DESC td{};
        td.Width  = static_cast<UINT>(width);
        td.Height = static_cast<UINT>(height);
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R24G8_TYPELESS;
        td.SampleDesc.Count   = actual_samples;
        td.SampleDesc.Quality = use_msaa ? msaa_quality : 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        if (!use_msaa) td.BindFlags |= D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = device_->CreateTexture2D(&td, nullptr, rt.depth_texture.GetAddressOf());
        if (FAILED(hr)) return 0;

        D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc{};
        dsv_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsv_desc.ViewDimension = use_msaa ? D3D11_DSV_DIMENSION_TEXTURE2DMS : D3D11_DSV_DIMENSION_TEXTURE2D;
        hr = device_->CreateDepthStencilView(rt.depth_texture.Get(), &dsv_desc, rt.depth_dsv.GetAddressOf());
        if (FAILED(hr)) return 0;

        if (!use_msaa) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
            srv_desc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;
            hr = device_->CreateShaderResourceView(rt.depth_texture.Get(), &srv_desc,
                                                    rt.depth_srv.GetAddressOf());
            if (FAILED(hr)) return 0;

            DX11Texture depth_tex;
            depth_tex.texture = rt.depth_texture;
            depth_tex.srv     = rt.depth_srv;
            depth_tex.width   = width;
            depth_tex.height  = height;
            rt.depth_texture_handle = next_texture_handle_++;
            textures_[rt.depth_texture_handle] = std::move(depth_tex);
        }
    }

    unsigned int handle = next_render_target_handle_++;
    render_targets_[handle] = std::move(rt);
    return handle;
}

void DX11ResourceManager::DeleteRenderTarget(unsigned int handle) {
    auto it = render_targets_.find(handle);
    if (it == render_targets_.end()) return;

    auto& rt = it->second;
    if (!rt.color_texture_handles_mrt.empty()) {
        for (auto h : rt.color_texture_handles_mrt)
            if (h) textures_.erase(h);
    } else {
        if (rt.color_texture_handle) textures_.erase(rt.color_texture_handle);
    }
    if (rt.depth_texture_handle) textures_.erase(rt.depth_texture_handle);

    render_targets_.erase(it);
}

const DX11RenderTarget* DX11ResourceManager::GetRenderTarget(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? &it->second : nullptr;
}

unsigned int DX11ResourceManager::GetRenderTargetColorTextureHandle(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? it->second.color_texture_handle : 0;
}

unsigned int DX11ResourceManager::GetRenderTargetDepthTextureHandle(unsigned int handle) const {
    auto it = render_targets_.find(handle);
    return it != render_targets_.end() ? it->second.depth_texture_handle : 0;
}

DX11ResourceManager::ReadbackResult DX11ResourceManager::ReadRenderTargetColor(unsigned int handle) const {
    ReadbackResult result;
    auto it = render_targets_.find(handle);
    if (it == render_targets_.end() || !it->second.color_texture) return result;

    auto& rt = it->second;
    result.width = rt.width;
    result.height = rt.height;

    // 创建 staging 纹理用于回读
    D3D11_TEXTURE2D_DESC td{};
    rt.color_texture->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    td.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = device_->CreateTexture2D(&td, nullptr, staging.GetAddressOf());
    if (FAILED(hr)) return result;

    dc_->CopyResource(staging.Get(), rt.color_texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = dc_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return result;

    result.pixels.resize(rt.width * rt.height * 4);
    const unsigned char* src = static_cast<const unsigned char*>(mapped.pData);
    if (td.Format == DXGI_FORMAT_R16G16B16A16_FLOAT) {
        // PBR shader 输出已在 gamma 空间，直接 clamp [0,1] → uint8
        for (int y = 0; y < rt.height; ++y) {
            const uint16_t* row = reinterpret_cast<const uint16_t*>(src + y * mapped.RowPitch);
            unsigned char* dst = result.pixels.data() + y * rt.width * 4;
            for (int x = 0; x < rt.width; ++x) {
                for (int c = 0; c < 4; ++c) {
                    float f = HalfToFloat(row[x * 4 + c]);
                    f = (std::max)(0.0f, (std::min)(1.0f, f));
                    dst[x * 4 + c] = static_cast<unsigned char>(f * 255.0f + 0.5f);
                }
            }
        }
    } else {
        for (int y = 0; y < rt.height; ++y) {
            std::memcpy(result.pixels.data() + y * rt.width * 4,
                   src + y * mapped.RowPitch,
                   rt.width * 4);
        }
    }
    dc_->Unmap(staging.Get(), 0);

    return result;
}

DX11ResourceManager::DepthReadbackResult DX11ResourceManager::ReadRenderTargetDepth(unsigned int handle) const {
    DepthReadbackResult result;
    auto it = render_targets_.find(handle);
    if (it == render_targets_.end() || !it->second.has_depth || !it->second.depth_texture) return result;

    auto& rt = it->second;
    result.width = rt.width;
    result.height = rt.height;

    // staging 纹理（与深度纹理同 desc：R24G8_TYPELESS）用于回读。
    D3D11_TEXTURE2D_DESC td{};
    rt.depth_texture->GetDesc(&td);
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    td.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = device_->CreateTexture2D(&td, nullptr, staging.GetAddressOf());
    if (FAILED(hr)) return {};

    dc_->CopyResource(staging.Get(), rt.depth_texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = dc_->Map(staging.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) return {};

    // D24_UNORM_S8：每 texel 32 位，低 24 位深度（R24_UNORM），高 8 位模板。
    constexpr float kInv24 = 1.0f / 16777215.0f;  // 1/(2^24-1)
    result.depth.resize(static_cast<size_t>(rt.width) * rt.height, 1.0f);
    const unsigned char* src = static_cast<const unsigned char*>(mapped.pData);
    for (int y = 0; y < rt.height; ++y) {
        const uint32_t* row = reinterpret_cast<const uint32_t*>(src + y * mapped.RowPitch);
        float* dst = result.depth.data() + static_cast<size_t>(y) * rt.width;
        for (int x = 0; x < rt.width; ++x) {
            dst[x] = static_cast<float>(row[x] & 0x00FFFFFFu) * kInv24;
        }
    }
    dc_->Unmap(staging.Get(), 0);

    return result;
}

// ============================================================
// 顶点数组（D3D11 无 VAO 概念，占位实现）
// ============================================================

dse::render::VertexArrayHandle DX11ResourceManager::CreateVertexArray() {
    unsigned int handle = next_vao_handle_++;
    vertex_arrays_[handle] = DX11VertexArray{handle};
    return dse::render::VertexArrayHandle{handle};
}

void DX11ResourceManager::DeleteVertexArray(dse::render::VertexArrayHandle handle) {
    vertex_arrays_.erase(handle.raw());
}

// ============================================================
// 异步纹理上传
// ============================================================

unsigned int DX11ResourceManager::CreateTexture2DAsync(int width, int height) {
    if (!device_) return 0;

    // 主线程创建空 GPU 纹理（内容由后续 FlushPendingUploads 填充）
    const bool use_hdr = context_ ? context_->hdr_enabled() : false;
    const DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM; // 纹理始终用 8 位色
    (void)use_hdr;

    D3D11_TEXTURE2D_DESC td{};
    td.Width  = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    ComPtr<ID3D11Texture2D> gpu_tex;
    HRESULT hr = device_->CreateTexture2D(&td, nullptr, gpu_tex.GetAddressOf());
    if (FAILED(hr)) return 0;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = fmt;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    DX11Texture tex;
    hr = device_->CreateShaderResourceView(gpu_tex.Get(), &srv_desc, tex.srv.GetAddressOf());
    if (FAILED(hr)) return 0;

    tex.texture = gpu_tex;
    tex.width   = width;
    tex.height  = height;

    unsigned int handle = next_texture_handle_++;
    textures_[handle] = std::move(tex);
    return handle;
}

void DX11ResourceManager::QueueTextureUpload(unsigned int handle, int width, int height,
                                              const unsigned char* rgba8_data) {
    if (!device_ || !rgba8_data) return;

    // 工作线程只写 CPU staging 纹理，不调用 D3D11 API
    D3D11_TEXTURE2D_DESC td{};
    td.Width  = static_cast<UINT>(width);
    td.Height = static_cast<UINT>(height);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    D3D11_SUBRESOURCE_DATA init_data{};
    init_data.pSysMem = rgba8_data;
    init_data.SysMemPitch = static_cast<UINT>(width * 4);

    ComPtr<ID3D11Texture2D> staging;
    HRESULT hr = device_->CreateTexture2D(&td, &init_data, staging.GetAddressOf());
    if (FAILED(hr)) return;

    PendingUpload upload;
    upload.handle  = handle;
    upload.staging = staging;
    upload.width   = width;
    upload.height  = height;

    std::lock_guard<std::mutex> lock(pending_uploads_mutex_);
    pending_uploads_.push(std::move(upload));
}

void DX11ResourceManager::FlushPendingUploads() {
    if (!dc_) return;

    std::lock_guard<std::mutex> lock(pending_uploads_mutex_);
    while (!pending_uploads_.empty()) {
        auto& upload = pending_uploads_.front();

        auto it = textures_.find(upload.handle);
        if (it != textures_.end() && it->second.texture && upload.staging) {
            dc_->CopyResource(it->second.texture.Get(), upload.staging.Get());
        }

        pending_uploads_.pop();
    }
}

// ============================================================
// Indirect Draw Buffer
// ============================================================

unsigned int DX11ResourceManager::CreateIndirectBuffer(size_t size, const void* data) {
    if (!device_ || size == 0) return 0;
    DX11IndirectBuffer buf;
    buf.size = size;

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = static_cast<UINT>(size);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = 0;  // DX11 spec: MISC_DRAWINDIRECT_ARGS 不允许任何 BindFlags
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
    bd.StructureByteStride = 0;

    D3D11_SUBRESOURCE_DATA sd{};
    sd.pSysMem = data;
    HRESULT hr = device_->CreateBuffer(&bd, data ? &sd : nullptr, &buf.buffer);
    if (FAILED(hr)) {
        DEBUG_LOG_WARN("[D3D11] CreateIndirectBuffer failed: size={}", size);
        return 0;
    }
    unsigned int handle = next_indirect_handle_++;
    indirect_buffers_[handle] = std::move(buf);
    return handle;
}

void DX11ResourceManager::UpdateIndirectBuffer(unsigned int handle,
                                                size_t offset, size_t size,
                                                const void* data) {
    if (!dc_ || !data) return;
    auto it = indirect_buffers_.find(handle);
    if (it == indirect_buffers_.end() || !it->second.buffer) return;
    D3D11_MAPPED_SUBRESOURCE mapped{};
    HRESULT hr = dc_->Map(it->second.buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    if (FAILED(hr)) return;
    std::memcpy(static_cast<unsigned char*>(mapped.pData) + offset, data, size);
    dc_->Unmap(it->second.buffer.Get(), 0);
}

void DX11ResourceManager::DeleteIndirectBuffer(unsigned int handle) {
    indirect_buffers_.erase(handle);
}

const DX11IndirectBuffer* DX11ResourceManager::GetIndirectBuffer(unsigned int handle) const {
    auto it = indirect_buffers_.find(handle);
    return (it != indirect_buffers_.end()) ? &it->second : nullptr;
}

} // namespace render
} // namespace dse
