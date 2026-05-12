/**
 * @file dx11_shader_manager.cpp
 * @brief DX11ShaderManager 实现 — HLSL 运行时编译与着色器程序管理
 */

#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/rhi/dx11/dx11_shader_sources.h"
#include "engine/base/debug.h"

namespace dse {
namespace render {

void DX11ShaderManager::Init(DX11Context* context) {
    context_ = context;
    DEBUG_LOG_INFO("[D3D11] ShaderManager initialized");
}

void DX11ShaderManager::Shutdown() {
    input_layouts_.clear();
    programs_.clear();
    programs_created_ = 0;
    programs_destroyed_ = 0;
    pbr_shader_handle_ = 0;
    skybox_shader_handle_ = 0;
    particle_shader_handle_ = 0;
    sprite_shader_handle_ = 0;
    postprocess_shader_handle_ = 0;
    shadow_shader_handle_ = 0;
    bloom_composite_shader_handle_ = 0;
    bloom_composite_ssao_shader_handle_ = 0;
    fxaa_shader_handle_ = 0;
    ssao_shader_handle_ = 0;
    ssao_blur_shader_handle_ = 0;
    ssao_apply_shader_handle_ = 0;
    lum_compute_shader_handle_ = 0;
    lum_adapt_shader_handle_ = 0;
    tonemapping_shader_handle_ = 0;
    bloom_composite_ssao_ae_shader_handle_ = 0;
    DEBUG_LOG_INFO("[D3D11] ShaderManager shutdown");
}

ComPtr<ID3DBlob> DX11ShaderManager::CompileShader(const std::string& source,
                                                     const std::string& entry_point,
                                                     const std::string& target) {
    ComPtr<ID3DBlob> blob;
    ComPtr<ID3DBlob> error_blob;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    HRESULT hr = D3DCompile(source.c_str(), source.size(), nullptr, nullptr, nullptr,
                             entry_point.c_str(), target.c_str(), flags, 0,
                             blob.GetAddressOf(), error_blob.GetAddressOf());

    if (FAILED(hr)) {
        if (error_blob) {
            DEBUG_LOG_ERROR("[D3D11] Shader compile error: {}",
                           static_cast<const char*>(error_blob->GetBufferPointer()));
        }
        return nullptr;
    }
    return blob;
}

unsigned int DX11ShaderManager::CreateProgram(const std::string& vert_src, const std::string& frag_src) {
    if (!context_) return 0;

    auto vs_blob = CompileShader(vert_src, "VSMain", "vs_5_0");
    if (!vs_blob) return 0;

    auto ps_blob = CompileShader(frag_src, "PSMain", "ps_5_0");
    if (!ps_blob) return 0;

    DX11ShaderProgram program;
    program.vs_blob = vs_blob;
    program.ps_blob = ps_blob;

    ID3D11Device* device = context_->device();

    HRESULT hr = device->CreateVertexShader(vs_blob->GetBufferPointer(), vs_blob->GetBufferSize(),
                                             nullptr, program.vertex_shader.GetAddressOf());
    if (FAILED(hr)) return 0;

    hr = device->CreatePixelShader(ps_blob->GetBufferPointer(), ps_blob->GetBufferSize(),
                                    nullptr, program.pixel_shader.GetAddressOf());
    if (FAILED(hr)) return 0;

    unsigned int handle = next_handle_++;
    programs_[handle] = std::move(program);
    programs_created_++;
    return handle;
}

void DX11ShaderManager::DeleteProgram(unsigned int handle) {
    auto it = programs_.find(handle);
    if (it == programs_.end()) return;
    programs_.erase(it);
    programs_destroyed_++;
}

const DX11ShaderProgram* DX11ShaderManager::GetProgram(unsigned int handle) const {
    auto it = programs_.find(handle);
    return it != programs_.end() ? &it->second : nullptr;
}

void DX11ShaderManager::CreateInputLayoutForShader(unsigned int handle,
                                                     const D3D11_INPUT_ELEMENT_DESC* elements,
                                                     UINT count) {
    auto it = programs_.find(handle);
    if (it == programs_.end() || !it->second.vs_blob) return;

    ComPtr<ID3D11InputLayout> layout;
    HRESULT hr = context_->device()->CreateInputLayout(
        elements, count,
        it->second.vs_blob->GetBufferPointer(),
        it->second.vs_blob->GetBufferSize(),
        layout.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateInputLayout failed for shader {}: 0x{:08X}",
                        handle, static_cast<unsigned>(hr));
        return;
    }
    input_layouts_[handle] = std::move(layout);
}

ID3D11InputLayout* DX11ShaderManager::GetInputLayout(unsigned int shader_handle) const {
    auto it = input_layouts_.find(shader_handle);
    return it != input_layouts_.end() ? it->second.Get() : nullptr;
}

void DX11ShaderManager::InitBuiltinShaders() {
    // ---- 精灵着色器 ----
    sprite_shader_handle_ = CreateProgram(dx11_shaders::kSpriteVS, dx11_shaders::kSpritePS);
    if (sprite_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin sprite shader created: {}", sprite_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(sprite_shader_handle_, layout, 3);
    }

    // ---- PBR 着色器 ----
    pbr_shader_handle_ = CreateProgram(dx11_shaders::kPbrVS,
        std::string(dx11_shaders::kPbrPS_Part1) + dx11_shaders::kPbrPS_Part2);
    if (pbr_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin PBR shader created: {}", pbr_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(pbr_shader_handle_, layout, 7);
    }

    // ---- 天空盒着色器 ----
    skybox_shader_handle_ = CreateProgram(dx11_shaders::kSkyboxVS, dx11_shaders::kSkyboxPS);
    if (skybox_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin skybox shader created: {}", skybox_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(skybox_shader_handle_, layout, 1);
    }

    // ---- 粒子着色器 ----
    particle_shader_handle_ = CreateProgram(dx11_shaders::kParticleVS, dx11_shaders::kParticlePS);
    if (particle_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin particle shader created: {}", particle_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            // Per-vertex (slot 0): float3 pos + float2 uv
            {"POSITION",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
            {"TEXCOORD",   0, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
            // Per-instance (slot 1): float3 iPos + float4 iCol + float iSize
            {"INST_POS",   0, DXGI_FORMAT_R32G32B32_FLOAT,    1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"INST_COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"INST_SIZE",  0, DXGI_FORMAT_R32_FLOAT,           1, 28, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        };
        CreateInputLayoutForShader(particle_shader_handle_, layout, 5);
    }

    // ---- 后处理着色器 ----
    postprocess_shader_handle_ = CreateProgram(dx11_shaders::kPostProcessVS, dx11_shaders::kPostProcessPassthroughPS);
    if (postprocess_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin postprocess shader created: {}", postprocess_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(postprocess_shader_handle_, layout, 2);
    }

    // ---- 阴影着色器 ----
    shadow_shader_handle_ = CreateProgram(dx11_shaders::kShadowVS, dx11_shaders::kShadowPS);
    if (shadow_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin shadow shader created: {}", shadow_shader_handle_);
        // 复用 PBR 顶点格式（BatchVertex）
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"COLOR",        0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT",      0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(shadow_shader_handle_, layout, 7);
    }

    // ---- Bloom Compute Shaders ----
    bloom_downsample_cs_handle_ = CreateComputeProgram(dx11_shaders::kBloomDownsampleCS);
    if (bloom_downsample_cs_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin bloom downsample CS created: {}", bloom_downsample_cs_handle_);

    bloom_upsample_cs_handle_ = CreateComputeProgram(dx11_shaders::kBloomUpsampleCS);
    if (bloom_upsample_cs_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin bloom upsample CS created: {}", bloom_upsample_cs_handle_);

    // ---- Bloom Composite 着色器（ACES Filmic Tone Mapping）----
    bloom_composite_shader_handle_ = CreateProgram(dx11_shaders::kPostProcessVS, dx11_shaders::kBloomCompositePS);
    if (bloom_composite_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin bloom composite shader created: {}", bloom_composite_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(bloom_composite_shader_handle_, layout, 2);
    }

    // ---- FXAA shader ----
    auto create_pp_shader = [&](const char* ps_src, const char* name) -> unsigned int {
        unsigned int h = CreateProgram(dx11_shaders::kPostProcessVS, ps_src);
        if (h) {
            DEBUG_LOG_INFO("[D3D11] Builtin {} shader created: {}", name, h);
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
            };
            CreateInputLayoutForShader(h, layout, 2);
        }
        return h;
    };
    fxaa_shader_handle_ = create_pp_shader(dx11_shaders::kFxaaPS, "fxaa");
    ssao_shader_handle_ = create_pp_shader(dx11_shaders::kSsaoPS, "ssao");
    ssao_blur_shader_handle_ = create_pp_shader(dx11_shaders::kSsaoBlurPS, "ssao_blur");
    ssao_apply_shader_handle_ = create_pp_shader(dx11_shaders::kSsaoApplyPS, "ssao_apply");
    bloom_composite_ssao_shader_handle_ = create_pp_shader(dx11_shaders::kBloomCompositeSsaoPS, "bloom_composite_ssao");
    lum_compute_shader_handle_ = create_pp_shader(dx11_shaders::kLumComputePS, "lum_compute");
    lum_adapt_shader_handle_ = create_pp_shader(dx11_shaders::kLumAdaptPS, "lum_adapt");
    tonemapping_shader_handle_ = create_pp_shader(dx11_shaders::kTonemappingPS, "tonemapping");
    bloom_composite_ssao_ae_shader_handle_ = create_pp_shader(dx11_shaders::kBloomCompositeSsaoAePS, "bloom_composite_ssao_ae");
}

// ============================================================
// Compute Shader 管理
// ============================================================

unsigned int DX11ShaderManager::CreateComputeProgram(const std::string& cs_src) {
    if (!context_) return 0;
    ID3D11Device* device = context_->device();
    if (!device) return 0;

    auto cs_blob = CompileShader(cs_src, "CSMain", "cs_5_0");
    if (!cs_blob) return 0;

    DX11ComputeProgram prog;
    HRESULT hr = device->CreateComputeShader(cs_blob->GetBufferPointer(), cs_blob->GetBufferSize(),
                                              nullptr, prog.cs.GetAddressOf());
    if (FAILED(hr)) {
        DEBUG_LOG_ERROR("[D3D11] CreateComputeShader failed: 0x{:08X}", static_cast<unsigned>(hr));
        return 0;
    }

    // 创建 BloomParams 常量缓冲（float2 + float2 = 16 字节）
    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth = 16;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    device->CreateBuffer(&bd, nullptr, prog.params_cb.GetAddressOf());

    unsigned int handle = next_cs_handle_++;
    compute_programs_[handle] = std::move(prog);
    ++programs_created_;
    return handle;
}

const DX11ComputeProgram* DX11ShaderManager::GetComputeProgram(unsigned int handle) const {
    auto it = compute_programs_.find(handle);
    return it != compute_programs_.end() ? &it->second : nullptr;
}

} // namespace render
} // namespace dse
