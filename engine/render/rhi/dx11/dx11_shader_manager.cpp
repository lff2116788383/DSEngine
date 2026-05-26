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
#include "engine/render/shaders/generated/embed/pbr_gpu_driven_vert.gen.h"

// Reflection metadata for automated InputLayout creation
#include "engine/render/shaders/generated/embed/pbr_vert_reflect.gen.h"
#include "engine/render/shaders/generated/embed/pbr_frag_reflect.gen.h"
#include "engine/render/shaders/generated/embed/shadow_vert_reflect.gen.h"
#include "engine/render/shaders/generated/embed/pbr_gpu_driven_vert_reflect.gen.h"
#include "engine/render/shaders/generated/embed/sprite_vert_reflect.gen.h"
#include "engine/render/shaders/generated/embed/skybox_vert_reflect.gen.h"
#include "engine/render/shaders/generated/embed/postprocess_vert_reflect.gen.h"
#include "engine/render/shader_reflection.h"

#include "engine/base/debug.h"

#include <fstream>
#include <filesystem>
#include <functional>
#include <cstring>

namespace dse {
namespace render {

// 从 reflection 数据自动生成 D3D11 InputLayout
static void CreateInputLayoutFromReflection(
    const shader_reflect::StageReflection& vert_reflection,
    std::vector<D3D11_INPUT_ELEMENT_DESC>& out_layout) {
    out_layout.clear();
    uint32_t byte_offset = 0;
    for (uint32_t i = 0; i < vert_reflection.input_count; ++i) {
        const auto& input = vert_reflection.inputs[i];
        DXGI_FORMAT fmt = DXGI_FORMAT_R32G32B32A32_FLOAT;
        switch (input.type) {
            case shader_reflect::BaseType::Float: fmt = DXGI_FORMAT_R32_FLOAT; break;
            case shader_reflect::BaseType::Vec2:  fmt = DXGI_FORMAT_R32G32_FLOAT; break;
            case shader_reflect::BaseType::Vec3:  fmt = DXGI_FORMAT_R32G32B32_FLOAT; break;
            case shader_reflect::BaseType::Vec4:  fmt = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
            case shader_reflect::BaseType::Int:   fmt = DXGI_FORMAT_R32_SINT; break;
            case shader_reflect::BaseType::IVec4: fmt = DXGI_FORMAT_R32G32B32A32_SINT; break;
            default: fmt = DXGI_FORMAT_R32G32B32A32_FLOAT; break;
        }
        if (input.columns > 1) {
            DXGI_FORMAT col_fmt = (input.vec_size == 4) ? DXGI_FORMAT_R32G32B32A32_FLOAT :
                                  (input.vec_size == 3) ? DXGI_FORMAT_R32G32B32_FLOAT :
                                  (input.vec_size == 2) ? DXGI_FORMAT_R32G32_FLOAT :
                                                          DXGI_FORMAT_R32_FLOAT;
            uint32_t col_size = input.vec_size * 4;
            for (uint32_t c = 0; c < input.columns; ++c) {
                out_layout.push_back({"TEXCOORD", input.location + c, col_fmt,
                    0, byte_offset, D3D11_INPUT_PER_VERTEX_DATA, 0});
                byte_offset += col_size;
            }
        } else {
            out_layout.push_back({"TEXCOORD", input.location, fmt,
                0, byte_offset, D3D11_INPUT_PER_VERTEX_DATA, 0});
            byte_offset += input.byte_size;
        }
    }
}

static void AppendInstanceModelInputLayout(std::vector<D3D11_INPUT_ELEMENT_DESC>& out_layout) {
    out_layout.push_back({"TEXCOORD", 8,  DXGI_FORMAT_R32G32B32A32_FLOAT, 1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1});
    out_layout.push_back({"TEXCOORD", 9,  DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1});
    out_layout.push_back({"TEXCOORD", 10, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1});
    out_layout.push_back({"TEXCOORD", 11, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1});
}

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

    if (error_blob) {
        DEBUG_LOG_WARN("[D3D11] Shader compile warnings: {}",
                       static_cast<const char*>(error_blob->GetBufferPointer()));
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

unsigned int DX11ShaderManager::CreateProgramFromDXBC(const uint8_t* vs_bytecode, size_t vs_size,
                                                        const uint8_t* ps_bytecode, size_t ps_size) {
    if (!context_ || !vs_bytecode || !ps_bytecode || vs_size == 0 || ps_size == 0) return 0;

    DX11ShaderProgram program;
    ID3D11Device* device = context_->device();

    // 创建 VS blob 用于 InputLayout
    D3DCreateBlob(vs_size, program.vs_blob.GetAddressOf());
    memcpy(program.vs_blob->GetBufferPointer(), vs_bytecode, vs_size);

    D3DCreateBlob(ps_size, program.ps_blob.GetAddressOf());
    memcpy(program.ps_blob->GetBufferPointer(), ps_bytecode, ps_size);

    HRESULT hr = device->CreateVertexShader(vs_bytecode, vs_size,
                                             nullptr, program.vertex_shader.GetAddressOf());
    if (FAILED(hr)) return 0;

    hr = device->CreatePixelShader(ps_bytecode, ps_size,
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
    using namespace generated_shaders;
    auto pulse = [&]() { if (keep_alive) keep_alive(); };

    // ---- 精灵着色器 (DXBC) ----
    sprite_shader_handle_ = CreateProgramFromDXBC(
        ksprite_vert_dxbc, ksprite_vert_dxbc_size,
        ksprite_frag_dxbc, ksprite_frag_dxbc_size);
    if (sprite_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin sprite shader created (DXBC): {}", sprite_shader_handle_);
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> sprite_layout;
        CreateInputLayoutFromReflection(ksprite_vert_reflection, sprite_layout);
        CreateInputLayoutForShader(sprite_shader_handle_, sprite_layout.data(),
                                   static_cast<int>(sprite_layout.size()));
    }

    pulse();
    // ---- PBR 着色器 (HLSL 运行时编译: 动态采样器数组不兼容 fxc 离线编译) ----
    pbr_shader_handle_ = CreateProgram(kpbr_vert_hlsl, kpbr_frag_hlsl, "main", "main");
    if (pbr_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin PBR shader created: {}", pbr_shader_handle_);
        // InputLayout from reflection data
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> pbr_layout;
        CreateInputLayoutFromReflection(kpbr_vert_reflection, pbr_layout);
        AppendInstanceModelInputLayout(pbr_layout);
        CreateInputLayoutForShader(pbr_shader_handle_, pbr_layout.data(),
                                   static_cast<int>(pbr_layout.size()));

        // 纹理 slot 自动计算（reflection 驱动）
        {
            using namespace dse::render::gl_reflect;
            std::vector<TextureUnitEntry> tex_entries;
            uint32_t next_unit = ComputeFlatTextureUnits(kpbr_frag_reflection, tex_entries);
            auto& slots = pbr_texture_slots_;
            slots.ssbo_base = static_cast<int>((next_unit < 16) ? 16 : next_unit);
            for (const auto& e : tex_entries) {
                int u = static_cast<int>(e.unit);
                if (std::strcmp(e.name, "u_texture") == 0)              slots.albedo = u;
                else if (std::strcmp(e.name, "u_normal_map") == 0)      slots.normal = u;
                else if (std::strcmp(e.name, "u_metallic_roughness_map") == 0) slots.metallic_roughness = u;
                else if (std::strcmp(e.name, "u_emissive_map") == 0)    slots.emissive = u;
                else if (std::strcmp(e.name, "u_occlusion_map") == 0)   slots.occlusion = u;
                else if (std::strcmp(e.name, "u_shadow_maps") == 0)     slots.shadow_base = u;
                else if (std::strcmp(e.name, "u_spot_shadow_maps") == 0) slots.spot_shadow_base = u;
                else if (std::strcmp(e.name, "u_reflection_cubemap") == 0) slots.reflection_cubemap = u;
                else if (std::strcmp(e.name, "u_brdf_lut") == 0)        slots.brdf_lut = u;
                else if (std::strcmp(e.name, "u_splat_weight_map") == 0) slots.splat_weight = u;
                else if (std::strcmp(e.name, "u_splat_layer0") == 0)    slots.splat_layer_base = u;
                else if (std::strcmp(e.name, "u_point_shadow_maps") == 0) slots.point_shadow_base = u;
            }

#ifndef NDEBUG
            shader_reflect_debug::ValidateTextureSlotOverlaps(tex_entries);
            shader_reflect_debug::ValidateUBOBindings(kpbr_frag_reflection, "DX11 PBR.frag");
            shader_reflect_debug::ValidateVertexInputs(kpbr_vert_reflection);
#endif
        }
    }

    // ---- 天空盒着色器 (DXBC) ----
    skybox_shader_handle_ = CreateProgramFromDXBC(
        kskybox_vert_dxbc, kskybox_vert_dxbc_size,
        kskybox_frag_dxbc, kskybox_frag_dxbc_size);
    if (skybox_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin skybox shader created (DXBC): {}", skybox_shader_handle_);
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> skybox_layout;
        CreateInputLayoutFromReflection(kskybox_vert_reflection, skybox_layout);
        CreateInputLayoutForShader(skybox_shader_handle_, skybox_layout.data(),
                                   static_cast<int>(skybox_layout.size()));
    }

    // ---- 粒子着色器 (DXBC) ----
    particle_shader_handle_ = CreateProgramFromDXBC(
        kparticle_vert_dxbc, kparticle_vert_dxbc_size,
        kparticle_frag_dxbc, kparticle_frag_dxbc_size);
    if (particle_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin particle shader created (DXBC): {}", particle_shader_handle_);
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA,   0},
            {"TEXCOORD", 1, DXGI_FORMAT_R32G32_FLOAT,       0, 12, D3D11_INPUT_PER_VERTEX_DATA,   0},
            {"TEXCOORD", 2, DXGI_FORMAT_R32G32B32_FLOAT,    1,  0, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"TEXCOORD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 12, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"TEXCOORD", 4, DXGI_FORMAT_R32_FLOAT,           1, 28, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        };
        CreateInputLayoutForShader(particle_shader_handle_, layout, 5);
    }

    // ---- 后处理着色器 (DXBC) ----
    postprocess_shader_handle_ = CreateProgramFromDXBC(
        kpostprocess_vert_dxbc, kpostprocess_vert_dxbc_size,
        kpostprocess_passthrough_frag_dxbc, kpostprocess_passthrough_frag_dxbc_size);
    if (postprocess_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin postprocess shader created (DXBC): {}", postprocess_shader_handle_);
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> pp_layout;
        CreateInputLayoutFromReflection(kpostprocess_vert_reflection, pp_layout);
        CreateInputLayoutForShader(postprocess_shader_handle_, pp_layout.data(),
                                   static_cast<int>(pp_layout.size()));
    }

    pulse();
    // ---- 阴影着色器 (DXBC) ----
    shadow_shader_handle_ = CreateProgramFromDXBC(
        kshadow_vert_dxbc, kshadow_vert_dxbc_size,
        kshadow_frag_dxbc, kshadow_frag_dxbc_size);
    if (shadow_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin shadow shader created (DXBC): {}", shadow_shader_handle_);
        // InputLayout from reflection data
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> shadow_layout;
        CreateInputLayoutFromReflection(kshadow_vert_reflection, shadow_layout);
        AppendInstanceModelInputLayout(shadow_layout);
        CreateInputLayoutForShader(shadow_shader_handle_, shadow_layout.data(),
                                   static_cast<int>(shadow_layout.size()));
    }

    // ---- Bloom Compute Shaders (仍用 HLSL 编译，CS 无 DXBC 通用路径) ----
    bloom_downsample_cs_handle_ = CreateComputeProgram(kbloom_downsample_comp_hlsl, "main");
    if (bloom_downsample_cs_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin bloom downsample CS created: {}", bloom_downsample_cs_handle_);

    bloom_upsample_cs_handle_ = CreateComputeProgram(kbloom_upsample_comp_hlsl, "main");
    if (bloom_upsample_cs_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin bloom upsample CS created: {}", bloom_upsample_cs_handle_);

    // ---- 后处理着色器 DXBC 批量创建 lambda ----
    auto create_pp_dxbc = [&](const uint8_t* ps_dxbc, size_t ps_dxbc_sz, const char* name) -> unsigned int {
        unsigned int h = CreateProgramFromDXBC(
            kpostprocess_vert_dxbc, kpostprocess_vert_dxbc_size,
            ps_dxbc, ps_dxbc_sz);
        if (h) {
            DEBUG_LOG_INFO("[D3D11] Builtin {} shader created (DXBC): {}", name, h);
            using namespace generated_shaders::reflect;
            std::vector<D3D11_INPUT_ELEMENT_DESC> pp_layout;
            CreateInputLayoutFromReflection(kpostprocess_vert_reflection, pp_layout);
            CreateInputLayoutForShader(h, pp_layout.data(),
                                       static_cast<int>(pp_layout.size()));
        }
        return h;
    };

    pulse();
    bloom_composite_shader_handle_ = create_pp_dxbc(kbloom_composite_frag_dxbc, kbloom_composite_frag_dxbc_size, "bloom_composite");
    bloom_extract_shader_handle_ = create_pp_dxbc(kbloom_extract_frag_dxbc, kbloom_extract_frag_dxbc_size, "bloom_extract");
    fxaa_shader_handle_ = create_pp_dxbc(kfxaa_frag_dxbc, kfxaa_frag_dxbc_size, "fxaa");
    ssao_shader_handle_ = create_pp_dxbc(kssao_frag_dxbc, kssao_frag_dxbc_size, "ssao");
    ssao_blur_shader_handle_ = create_pp_dxbc(kssao_blur_frag_dxbc, kssao_blur_frag_dxbc_size, "ssao_blur");
    ssao_apply_shader_handle_ = create_pp_dxbc(kssao_apply_frag_dxbc, kssao_apply_frag_dxbc_size, "ssao_apply");
    contact_shadow_shader_handle_ = create_pp_dxbc(kcontact_shadow_frag_dxbc, kcontact_shadow_frag_dxbc_size, "contact_shadow");
    bloom_composite_ssao_shader_handle_ = create_pp_dxbc(kbloom_composite_ssao_frag_dxbc, kbloom_composite_ssao_frag_dxbc_size, "bloom_composite_ssao");
    lum_compute_shader_handle_ = create_pp_dxbc(klum_compute_frag_dxbc, klum_compute_frag_dxbc_size, "lum_compute");
    lum_adapt_shader_handle_ = create_pp_dxbc(klum_adapt_frag_dxbc, klum_adapt_frag_dxbc_size, "lum_adapt");
    pulse();
    tonemapping_shader_handle_ = create_pp_dxbc(ktonemapping_frag_dxbc, ktonemapping_frag_dxbc_size, "tonemapping");
    bloom_composite_ssao_ae_shader_handle_ = create_pp_dxbc(kbloom_composite_ssao_ae_frag_dxbc, kbloom_composite_ssao_ae_frag_dxbc_size, "bloom_composite_ssao_ae");
    color_grading_shader_handle_ = create_pp_dxbc(kcolor_grading_frag_dxbc, kcolor_grading_frag_dxbc_size, "color_grading");
    taa_resolve_shader_handle_ = create_pp_dxbc(ktaa_resolve_frag_dxbc, ktaa_resolve_frag_dxbc_size, "taa_resolve");
    dof_shader_handle_ = create_pp_dxbc(kdof_frag_dxbc, kdof_frag_dxbc_size, "dof");
    pulse();
    motion_blur_shader_handle_ = create_pp_dxbc(kmotion_blur_frag_dxbc, kmotion_blur_frag_dxbc_size, "motion_blur");
    ssr_shader_handle_ = create_pp_dxbc(kssr_frag_dxbc, kssr_frag_dxbc_size, "ssr");
    motion_vector_shader_handle_ = create_pp_dxbc(kmotion_vector_frag_dxbc, kmotion_vector_frag_dxbc_size, "motion_vector");
    deferred_lighting_shader_handle_ = create_pp_dxbc(kdeferred_lighting_frag_dxbc, kdeferred_lighting_frag_dxbc_size, "deferred_lighting");
    edge_detect_shader_handle_ = create_pp_dxbc(kedge_detect_frag_dxbc, kedge_detect_frag_dxbc_size, "edge_detect");
    volumetric_fog_shader_handle_ = create_pp_dxbc(kvolumetric_fog_frag_dxbc, kvolumetric_fog_frag_dxbc_size, "volumetric_fog");
    decal_shader_handle_ = create_pp_dxbc(kdecal_frag_dxbc, kdecal_frag_dxbc_size, "decal");
    wboit_composite_shader_handle_ = create_pp_dxbc(kwboit_composite_frag_dxbc, kwboit_composite_frag_dxbc_size, "wboit_composite");
    water_shader_handle_ = create_pp_dxbc(kwater_frag_dxbc, kwater_frag_dxbc_size, "water");
    light_shaft_shader_handle_ = create_pp_dxbc(klight_shaft_frag_dxbc, klight_shaft_frag_dxbc_size, "light_shaft");

    pulse();
    // ---- GBuffer 着色器（复用 PBR VS DXBC + GBuffer PS DXBC）----
    gbuffer_shader_handle_ = CreateProgramFromDXBC(
        kpbr_vert_dxbc, kpbr_vert_dxbc_size,
        kgbuffer_frag_dxbc, kgbuffer_frag_dxbc_size);
    if (gbuffer_shader_handle_) {
        DEBUG_LOG_INFO("[D3D11] Builtin GBuffer shader created (DXBC): {}", gbuffer_shader_handle_);
        // GBuffer uses same vertex layout as PBR (shares VS)
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> gb_layout;
        CreateInputLayoutFromReflection(kpbr_vert_reflection, gb_layout);
        AppendInstanceModelInputLayout(gb_layout);
        CreateInputLayoutForShader(gbuffer_shader_handle_, gb_layout.data(),
                                   static_cast<int>(gb_layout.size()));
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

// ============================================================================
// GPU-Driven PBR Shader
// ============================================================================

void DX11ShaderManager::InitGPUDrivenPBRShader() {
    using namespace dse::render::generated_shaders;

    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    auto replace_first = [](std::string& s, const std::string& from, const std::string& to) {
        auto p = s.find(from);
        if (p != std::string::npos) s.replace(p, from.size(), to);
    };

    // --- VS patch: 添加 DrawIdCB (b7), 用 g_draw_id 替换 SV_InstanceID ---
    std::string vert_src = kpbr_gpu_driven_vert_hlsl;

    // 在 ByteAddressBuffer _33 声明行后注入 DrawIdCB（不硬编码 register slot）
    {
        const std::string marker = "ByteAddressBuffer ";
        auto mpos = vert_src.find(marker);
        if (mpos != std::string::npos) {
            auto eol = vert_src.find(';', mpos);
            if (eol != std::string::npos) {
                vert_src.insert(eol + 1, "\ncbuffer DrawIdCB : register(b7) { uint g_draw_id; };\n");
            }
        }
    }

    // 替换 SV_InstanceID 输入为不使用（用 g_draw_id 替代）
    // 在 SPIRV_Cross_Input 中保留 SV_InstanceID 声明（InputLayout 需要），
    // 但在 main() 中改用 g_draw_id
    replace_all(vert_src,
        "gl_InstanceIndex = int(stage_input.gl_InstanceIndex);",
        "gl_InstanceIndex = int(g_draw_id);");
    replace_all(vert_src, "gl_InstanceIndex * 80", "g_draw_id * 80");

    // --- PS: 使用标准 PBR PS（material 通过 CPU per-draw 更新 PerMaterial cbuffer b3）---
    std::string frag_src(kpbr_frag_hlsl);

    gpu_driven_pbr_shader_handle_ = CreateProgram(vert_src, frag_src, "main", "main");
    if (gpu_driven_pbr_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("[DX11] GPU-driven PBR shader compilation failed");
    } else {
        DEBUG_LOG_INFO("[DX11] GPU-driven PBR shader created: handle={}", gpu_driven_pbr_shader_handle_);
        using namespace generated_shaders::reflect;
        std::vector<D3D11_INPUT_ELEMENT_DESC> gpu_pbr_layout;
        CreateInputLayoutFromReflection(kpbr_gpu_driven_vert_reflection, gpu_pbr_layout);
        CreateInputLayoutForShader(gpu_driven_pbr_shader_handle_, gpu_pbr_layout.data(),
                                   static_cast<int>(gpu_pbr_layout.size()));
    }
}

// ============================================================================
// GPU-Driven Shadow Shader
// ============================================================================

void DX11ShaderManager::InitGPUDrivenShadowShader() {
    using namespace dse::render::generated_shaders;

    auto replace_all = [](std::string& s, const std::string& from, const std::string& to) {
        size_t pos = 0;
        while ((pos = s.find(from, pos)) != std::string::npos) {
            s.replace(pos, from.size(), to);
            pos += to.size();
        }
    };
    auto replace_first = [](std::string& s, const std::string& from, const std::string& to) {
        auto p = s.find(from);
        if (p != std::string::npos) s.replace(p, from.size(), to);
    };

    // Shadow VS: 从标准 shadow vert HLSL patch，替换 PushConstants u_model → ByteAddressBuffer fetch
    std::string vert_src(kshadow_vert_hlsl);

    // 注入 instance buffer 和 DrawIdCB
    replace_first(vert_src,
        "cbuffer PushConstants",
        "ByteAddressBuffer _33 : register(t21);\n"
        "cbuffer DrawIdCB : register(b7) { uint g_draw_id; };\n\n"
        "cbuffer PushConstants");

    // 替换 pc_u_model → instance buffer 读取
    // pc_u_model 是 row_major float4x4，对应 ByteAddressBuffer 偏移 g_draw_id * 80
    replace_all(vert_src, "mul(localPos, pc_u_model)",
        "mul(localPos, asfloat(uint4x4(_33.Load4(g_draw_id * 80 + 0), _33.Load4(g_draw_id * 80 + 16), _33.Load4(g_draw_id * 80 + 32), _33.Load4(g_draw_id * 80 + 48))))");
    replace_all(vert_src, "mul(boneTransform, pc_u_model)",
        "mul(boneTransform, asfloat(uint4x4(_33.Load4(g_draw_id * 80 + 0), _33.Load4(g_draw_id * 80 + 16), _33.Load4(g_draw_id * 80 + 32), _33.Load4(g_draw_id * 80 + 48))))");

    gpu_driven_shadow_shader_handle_ = CreateProgram(vert_src, std::string(kshadow_frag_hlsl), "main", "main");
    if (gpu_driven_shadow_shader_handle_ == 0) {
        DEBUG_LOG_ERROR("[DX11] GPU-driven Shadow shader compilation failed");
    } else {
        DEBUG_LOG_INFO("[DX11] GPU-driven Shadow shader created: handle={}", gpu_driven_shadow_shader_handle_);
        auto* shadow_layout = GetInputLayout(shadow_shader_handle_);
        if (shadow_layout) {
            input_layouts_[gpu_driven_shadow_shader_handle_] = shadow_layout;
        }
    }
}

} // namespace render
} // namespace dse
