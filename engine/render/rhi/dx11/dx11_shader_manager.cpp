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
    programs_.clear();
    programs_created_ = 0;
    programs_destroyed_ = 0;
    pbr_shader_handle_ = 0;
    skybox_shader_handle_ = 0;
    particle_shader_handle_ = 0;
    sprite_shader_handle_ = 0;
    postprocess_shader_handle_ = 0;
    shadow_shader_handle_ = 0;
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

void DX11ShaderManager::InitBuiltinShaders() {
    // 精灵着色器
    sprite_shader_handle_ = CreateProgram(dx11_shaders::kSpriteVS, dx11_shaders::kSpritePS);
    if (sprite_shader_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin sprite shader created: {}", sprite_shader_handle_);

    // PBR 着色器
    pbr_shader_handle_ = CreateProgram(dx11_shaders::kPbrVS, dx11_shaders::kPbrPS);
    if (pbr_shader_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin PBR shader created: {}", pbr_shader_handle_);

    // 天空盒着色器
    skybox_shader_handle_ = CreateProgram(dx11_shaders::kSkyboxVS, dx11_shaders::kSkyboxPS);
    if (skybox_shader_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin skybox shader created: {}", skybox_shader_handle_);

    // 粒子着色器
    particle_shader_handle_ = CreateProgram(dx11_shaders::kParticleVS, dx11_shaders::kParticlePS);
    if (particle_shader_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin particle shader created: {}", particle_shader_handle_);

    // 后处理着色器
    postprocess_shader_handle_ = CreateProgram(dx11_shaders::kPostProcessVS, dx11_shaders::kPostProcessPassthroughPS);
    if (postprocess_shader_handle_)
        DEBUG_LOG_INFO("[D3D11] Builtin postprocess shader created: {}", postprocess_shader_handle_);
}

} // namespace render
} // namespace dse
