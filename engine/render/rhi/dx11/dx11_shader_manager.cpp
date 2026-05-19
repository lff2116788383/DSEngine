/**
 * @file dx11_shader_manager.cpp
 * @brief DX11ShaderManager 实现 — HLSL 运行时编译与着色器程序管理
 */

#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/shaders/generated/embed/pbr_vert.gen.h"
#include "engine/render/shaders/generated/embed/pbr_frag.gen.h"
#include "engine/render/shaders/generated/embed/sprite_vert.gen.h"
#include "engine/render/shaders/generated/embed/sprite_frag.gen.h"
#include "engine/render/shaders/generated/embed/skybox_vert.gen.h"
#include "engine/render/shaders/generated/embed/skybox_frag.gen.h"
#include "engine/render/shaders/generated/embed/particle_vert.gen.h"
#include "engine/render/shaders/generated/embed/particle_frag.gen.h"
#include "engine/render/shaders/generated/embed/postprocess_vert.gen.h"
#include "engine/render/shaders/generated/embed/shadow_vert.gen.h"
#include "engine/render/shaders/generated/embed/shadow_frag.gen.h"
#include "engine/render/shaders/generated/embed/gbuffer_frag.gen.h"
#include "engine/render/shaders/generated/embed/fxaa_frag.gen.h"
#include "engine/render/shaders/generated/embed/bloom_extract_frag.gen.h"
#include "engine/render/shaders/generated/embed/bloom_composite_frag.gen.h"
#include "engine/render/shaders/generated/embed/bloom_downsample_comp.gen.h"
#include "engine/render/shaders/generated/embed/bloom_upsample_comp.gen.h"
#include "engine/render/shaders/generated/embed/postprocess_passthrough_frag.gen.h"
#include "engine/render/shaders/generated/embed/ssao_frag.gen.h"
#include "engine/render/shaders/generated/embed/ssao_blur_frag.gen.h"
#include "engine/render/shaders/generated/embed/ssao_apply_frag.gen.h"
#include "engine/render/shaders/generated/embed/contact_shadow_frag.gen.h"
#include "engine/render/shaders/generated/embed/bloom_composite_ssao_frag.gen.h"
#include "engine/render/shaders/generated/embed/bloom_composite_ssao_ae_frag.gen.h"
#include "engine/render/shaders/generated/embed/lum_compute_frag.gen.h"
#include "engine/render/shaders/generated/embed/lum_adapt_frag.gen.h"
#include "engine/render/shaders/generated/embed/tonemapping_frag.gen.h"
#include "engine/render/shaders/generated/embed/color_grading_frag.gen.h"
#include "engine/render/shaders/generated/embed/taa_resolve_frag.gen.h"
#include "engine/render/shaders/generated/embed/dof_frag.gen.h"
#include "engine/render/shaders/generated/embed/motion_blur_frag.gen.h"
#include "engine/render/shaders/generated/embed/ssr_frag.gen.h"
#include "engine/render/shaders/generated/embed/motion_vector_frag.gen.h"
#include "engine/render/shaders/generated/embed/deferred_lighting_frag.gen.h"
#include "engine/render/shaders/generated/embed/edge_detect_frag.gen.h"
#include "engine/render/shaders/generated/embed/volumetric_fog_frag.gen.h"
#include "engine/render/shaders/generated/embed/decal_frag.gen.h"
#include "engine/render/shaders/generated/embed/wboit_composite_frag.gen.h"
#include "engine/render/shaders/generated/embed/water_frag.gen.h"
#include "engine/render/shaders/generated/embed/light_shaft_frag.gen.h"
#include "engine/base/debug.h"

#include <fstream>
#include <filesystem>
#include <functional>

namespace dse {
namespace render {

static constexpr const char* kShaderCacheDir = "data/shader_cache";

static std::string ShaderCacheKey(const std::string& source,
                                   const std::string& entry_point,
                                   const std::string& target) {
    std::size_t h = std::hash<std::string>{}(source);
    h ^= std::hash<std::string>{}(entry_point) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<std::string>{}(target) + 0x9e3779b9 + (h << 6) + (h >> 2);
    char buf[32];
    snprintf(buf, sizeof(buf), "%016zx", h);
    return std::string(kShaderCacheDir) + "/" + buf + ".cso";
}

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
    contact_shadow_shader_handle_ = 0;
    lum_compute_shader_handle_ = 0;
    lum_adapt_shader_handle_ = 0;
    tonemapping_shader_handle_ = 0;
    bloom_composite_ssao_ae_shader_handle_ = 0;
    color_grading_shader_handle_ = 0;
    taa_resolve_shader_handle_ = 0;
    dof_shader_handle_ = 0;
    motion_blur_shader_handle_ = 0;
    ssr_shader_handle_ = 0;
    motion_vector_shader_handle_ = 0;
    gbuffer_shader_handle_ = 0;
    deferred_lighting_shader_handle_ = 0;
    edge_detect_shader_handle_ = 0;
    volumetric_fog_shader_handle_ = 0;
    decal_shader_handle_ = 0;
    DEBUG_LOG_INFO("[D3D11] ShaderManager shutdown");
}

ComPtr<ID3DBlob> DX11ShaderManager::CompileShader(const std::string& source,
                                                     const std::string& entry_point,
                                                     const std::string& target) {
    // 尝试从磁盘缓存加载已编译的 blob
    std::string cache_path = ShaderCacheKey(source, entry_point, target);
    {
        std::ifstream fin(cache_path, std::ios::binary | std::ios::ate);
        if (fin.is_open()) {
            auto size = fin.tellg();
            if (size > 0) {
                fin.seekg(0);
                ComPtr<ID3DBlob> blob;
                D3DCreateBlob(static_cast<SIZE_T>(size), blob.GetAddressOf());
                fin.read(static_cast<char*>(blob->GetBufferPointer()), size);
                if (fin.good()) {
                    return blob;
                }
            }
        }
    }

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

    // 写入磁盘缓存
    try {
        std::filesystem::create_directories(kShaderCacheDir);
        std::ofstream fout(cache_path, std::ios::binary);
        if (fout.is_open()) {
            fout.write(static_cast<const char*>(blob->GetBufferPointer()),
                       static_cast<std::streamsize>(blob->GetBufferSize()));
        }
    } catch (...) {}

    return blob;
}

unsigned int DX11ShaderManager::CreateProgram(const std::string& vert_src, const std::string& frag_src) {
    return CreateProgram(vert_src, frag_src, "VSMain", "PSMain");
}

unsigned int DX11ShaderManager::CreateProgram(const std::string& vert_src, const std::string& frag_src,
                                               const std::string& vs_entry, const std::string& ps_entry) {
    if (!context_) return 0;

    auto vs_blob = CompileShader(vert_src, vs_entry, "vs_5_0");
    if (!vs_blob) return 0;

    auto ps_blob = CompileShader(frag_src, ps_entry, "ps_5_0");
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

void DX11ShaderManager::InitBuiltinShaders(std::function<void()> keep_alive) {
    auto pulse = [&]() { if (keep_alive) keep_alive(); };
    // ---- 精灵着色器 ----
    sprite_shader_handle_ = CreateProgram(generated_shaders::ksprite_vert_hlsl, generated_shaders::ksprite_frag_hlsl, "main", "main");
    if (sprite_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin sprite shader created: {}", sprite_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(sprite_shader_handle_, layout, 3);
    }

    pulse();
    // ---- PBR 着色器 ----
    pbr_shader_handle_ = CreateProgram(generated_shaders::kpbr_vert_hlsl, generated_shaders::kpbr_frag_hlsl, "main", "main");
    if (pbr_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin PBR shader created: {}", pbr_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(pbr_shader_handle_, layout, 7);
    }

    // ---- 天空盒着色器 ----
    skybox_shader_handle_ = CreateProgram(generated_shaders::kskybox_vert_hlsl, generated_shaders::kskybox_frag_hlsl, "main", "main");
    if (skybox_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin skybox shader created: {}", skybox_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(skybox_shader_handle_, layout, 1);
    }

    // ---- 粒子着色器 ----
    particle_shader_handle_ = CreateProgram(generated_shaders::kparticle_vert_hlsl, generated_shaders::kparticle_frag_hlsl, "main", "main");
    if (particle_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin particle shader created: {}", particle_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            // Per-vertex (slot 0): float3 pos + float2 uv
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
            // Per-instance (slot 1): float3 iPos + float4 iCol + float iSize
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT,    1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT,           1, 28, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        };
        CreateInputLayoutForShader(particle_shader_handle_, layout, 5);
    }

    // ---- 后处理着色器 ----
    postprocess_shader_handle_ = CreateProgram(generated_shaders::kpostprocess_vert_hlsl, generated_shaders::kpostprocess_passthrough_frag_hlsl, "main", "main");
    if (postprocess_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin postprocess shader created: {}", postprocess_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(postprocess_shader_handle_, layout, 2);
    }

    pulse();
    // ---- 阴影着色器 ----
    shadow_shader_handle_ = CreateProgram(generated_shaders::kshadow_vert_hlsl, generated_shaders::kshadow_frag_hlsl, "main", "main");
    if (shadow_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin shadow shader created: {}", shadow_shader_handle_);
        // 复用 PBR 顶点格式（BatchVertex）+ GPU Instancing
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(shadow_shader_handle_, layout, 7);
    }

    // ---- Bloom Compute Shaders ----
    bloom_downsample_cs_handle_ = CreateComputeProgram(generated_shaders::kbloom_downsample_comp_hlsl, "main");
    if (bloom_downsample_cs_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin bloom downsample CS created (gen.h): {}", bloom_downsample_cs_handle_);

    bloom_upsample_cs_handle_ = CreateComputeProgram(generated_shaders::kbloom_upsample_comp_hlsl, "main");
    if (bloom_upsample_cs_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin bloom upsample CS created (gen.h): {}", bloom_upsample_cs_handle_);

    // ---- Bloom Composite 着色器（ACES Filmic Tone Mapping）----
    bloom_composite_shader_handle_ = CreateProgram(generated_shaders::kpostprocess_vert_hlsl,
                                                     generated_shaders::kbloom_composite_frag_hlsl,
                                                     "main", "main");
    if (bloom_composite_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin bloom composite shader created (gen.h): {}", bloom_composite_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(bloom_composite_shader_handle_, layout, 2);
    }

    // ---- gen.h shader 创建 lambda（VS 保留手写版，PS 入口点为 "main"）----
    auto create_pp_gen = [&](const char* ps_src, const char* name) -> unsigned int {
        unsigned int h = CreateProgram(generated_shaders::kpostprocess_vert_hlsl, ps_src, "main", "main");
        if (h) {
            DEBUG_LOG_INFO("[D3D11] Builtin {} shader created (gen.h): {}", name, h);
            D3D11_INPUT_ELEMENT_DESC layout[] = {
                {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT, 0,  8, D3D11_INPUT_PER_VERTEX_DATA, 0},
            };
            CreateInputLayoutForShader(h, layout, 2);
        }
        return h;
    };
    pulse();
    bloom_extract_shader_handle_ = create_pp_gen(generated_shaders::kbloom_extract_frag_hlsl, "bloom_extract");
    fxaa_shader_handle_ = create_pp_gen(generated_shaders::kfxaa_frag_hlsl, "fxaa");
    ssao_shader_handle_ = create_pp_gen(generated_shaders::kssao_frag_hlsl, "ssao");
    ssao_blur_shader_handle_ = create_pp_gen(generated_shaders::kssao_blur_frag_hlsl, "ssao_blur");
    ssao_apply_shader_handle_ = create_pp_gen(generated_shaders::kssao_apply_frag_hlsl, "ssao_apply");
    contact_shadow_shader_handle_ = create_pp_gen(generated_shaders::kcontact_shadow_frag_hlsl, "contact_shadow");
    bloom_composite_ssao_shader_handle_ = create_pp_gen(generated_shaders::kbloom_composite_ssao_frag_hlsl, "bloom_composite_ssao");
    lum_compute_shader_handle_ = create_pp_gen(generated_shaders::klum_compute_frag_hlsl, "lum_compute");
    lum_adapt_shader_handle_ = create_pp_gen(generated_shaders::klum_adapt_frag_hlsl, "lum_adapt");
    pulse();
    tonemapping_shader_handle_ = create_pp_gen(generated_shaders::ktonemapping_frag_hlsl, "tonemapping");
    bloom_composite_ssao_ae_shader_handle_ = create_pp_gen(generated_shaders::kbloom_composite_ssao_ae_frag_hlsl, "bloom_composite_ssao_ae");
    color_grading_shader_handle_ = create_pp_gen(generated_shaders::kcolor_grading_frag_hlsl, "color_grading");
    taa_resolve_shader_handle_ = create_pp_gen(generated_shaders::ktaa_resolve_frag_hlsl, "taa_resolve");
    dof_shader_handle_ = create_pp_gen(generated_shaders::kdof_frag_hlsl, "dof");
    pulse();
    motion_blur_shader_handle_ = create_pp_gen(generated_shaders::kmotion_blur_frag_hlsl, "motion_blur");
    ssr_shader_handle_ = create_pp_gen(generated_shaders::kssr_frag_hlsl, "ssr");
    motion_vector_shader_handle_ = create_pp_gen(generated_shaders::kmotion_vector_frag_hlsl, "motion_vector");
    deferred_lighting_shader_handle_ = create_pp_gen(generated_shaders::kdeferred_lighting_frag_hlsl, "deferred_lighting");
    edge_detect_shader_handle_ = create_pp_gen(generated_shaders::kedge_detect_frag_hlsl, "edge_detect");
    volumetric_fog_shader_handle_ = create_pp_gen(generated_shaders::kvolumetric_fog_frag_hlsl, "volumetric_fog");
    decal_shader_handle_ = create_pp_gen(generated_shaders::kdecal_frag_hlsl, "decal");
    wboit_composite_shader_handle_ = create_pp_gen(generated_shaders::kwboit_composite_frag_hlsl, "wboit_composite");
    water_shader_handle_ = create_pp_gen(generated_shaders::kwater_frag_hlsl, "water");
    light_shaft_shader_handle_ = create_pp_gen(generated_shaders::klight_shaft_frag_hlsl, "light_shaft");

    pulse();
    // ---- GBuffer 着色器（复用 PBR VS + GBuffer PS）----
    gbuffer_shader_handle_ = CreateProgram(generated_shaders::kpbr_vert_hlsl, generated_shaders::kgbuffer_frag_hlsl, "main", "main");
    if (gbuffer_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin GBuffer shader created: {}", gbuffer_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32_FLOAT,       0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32_FLOAT,    0, 36, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 4, DXGI_FORMAT_R32G32B32_FLOAT,    0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 5, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D11_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 6, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 76, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };
        CreateInputLayoutForShader(gbuffer_shader_handle_, layout, 7);
    }
}

// ============================================================
// Compute Shader 管理
// ============================================================

unsigned int DX11ShaderManager::CreateComputeProgram(const std::string& cs_src) {
    return CreateComputeProgram(cs_src, "CSMain");
}

unsigned int DX11ShaderManager::CreateComputeProgram(const std::string& cs_src, const std::string& cs_entry) {
    if (!context_) return 0;
    ID3D11Device* device = context_->device();
    if (!device) return 0;

    auto cs_blob = CompileShader(cs_src, cs_entry, "cs_5_0");
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
