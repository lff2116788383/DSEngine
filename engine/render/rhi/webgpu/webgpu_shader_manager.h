/**
 * @file webgpu_shader_manager.h
 * @brief WebGPU 着色器管理器（manager 拆分：依赖 ctx/res/pso）。
 *
 * 持有图形/计算着色器登记表、内建 WGSL 程序缓存、内建天空盒立方体 VBO、GPU-driven PBR
 * 程序/PSO/默认资源。WebGPU 无离线 GLSL→WGSL，各内建程序以手写 WGSL 源懒创建并缓存。
 */

#ifndef DSE_WEBGPU_SHADER_MANAGER_H
#define DSE_WEBGPU_SHADER_MANAGER_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/webgpu/webgpu_common.h"
#include "engine/render/rhi/webgpu/webgpu_context.h"
#include "engine/render/rhi/webgpu/webgpu_resource_manager.h"
#include "engine/render/rhi/webgpu/webgpu_pipeline_state_manager.h"

#include <set>
#include <string>
#include <unordered_map>

namespace dse {
namespace render {

class WebGPUShaderManager {
public:
    /// 注入依赖：ctx（设备/句柄发号）+ res（默认白纹理/UBO/天空盒 VBO）+ pso（PBR PSO）。
    void Init(WebGPUContext* ctx, WebGPUResourceManager* res, WebGPUPipelineStateManager* pso) {
        ctx_ = ctx; res_ = res; pso_ = pso;
    }
    /// 同名稳定句柄缓存：device 生命周期内不变（AcquireDevice 成功时设，Shutdown 时以空清）。
    void OnDeviceAcquired(WGPUDevice device, WGPUQueue queue) { device_ = device; queue_ = queue; }
    /// orchestrator Shutdown 调用：释放图形/计算着色器 module。
    void Shutdown();

    // --- 跨界访问器（draw 读）---
    const ShaderEntry* FindShader(unsigned int handle) const {
        auto it = shaders_.find(handle);
        return it == shaders_.end() ? nullptr : &it->second;
    }
    const ComputeShaderEntry* FindComputeShader(unsigned int handle) const {
        auto it = compute_shaders_.find(handle);
        return it == compute_shaders_.end() ? nullptr : &it->second;
    }
    unsigned int gpu_driven_pbr_program() const { return gpu_driven_pbr_program_; }
    unsigned int gpu_driven_pbr_pso() const { return gpu_driven_pbr_pso_; }
    BufferHandle gpu_driven_perframe_ubo() const { return gpu_driven_perframe_ubo_; }
    BufferHandle gpu_driven_perscene_ubo() const { return gpu_driven_perscene_ubo_; }
    unsigned int white_texture() const { return white_texture_; }

    // --- 迁自 device 的着色器方法（机械抽取）---
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src);
    void DeleteShaderProgram(unsigned int program_handle);
    unsigned int GetOrCreateWgslProgram(const std::string& key, const std::string& wgsl);
    unsigned int GetBuiltinProgram(BuiltinProgram program);
    unsigned int GetGenPPShaderProgram(const std::string& effect_name);
    unsigned int GetSkyboxCubeVertexBuffer();
    WGPUShaderModule CompileWGSL(const std::string& code, const char* label);
    bool EnsureGpuDrivenPBRShader();
    bool HasGPUDrivenPBRShader() const;
    unsigned int CreateComputeShader(const std::string& source);
    unsigned int CreateComputeShaderEx(const std::string& gl_src, const std::string& vk_src,
                                       const std::string& hlsl_src, uint32_t ssbo_count,
                                       uint32_t storage_image_count, uint32_t sampler_count,
                                       uint32_t push_constant_bytes, const std::string& wgsl_src);
    void DeleteComputeShader(unsigned int handle);

private:
    bool EnsureInitialized() { return ctx_->EnsureInitialized(); }
    unsigned int NextHandle() { return ctx_->NextHandle(); }

    WebGPUContext* ctx_ = nullptr;
    WebGPUResourceManager* res_ = nullptr;
    WebGPUPipelineStateManager* pso_ = nullptr;
    WGPUDevice device_ = nullptr;
    WGPUQueue queue_ = nullptr;

    std::unordered_map<unsigned int, ShaderEntry>        shaders_;
    std::unordered_map<unsigned int, ComputeShaderEntry> compute_shaders_;
    std::unordered_map<std::string, unsigned int> wgsl_program_cache_;  ///< 内建程序 key → 句柄
    unsigned int skybox_cube_vbo_ = 0;                   ///< 内建天空盒 36 顶点立方体 VBO（懒初始化）

    // GPU-driven PBR（手译 PBR WGSL：vs+fs 同源）+ 默认资源（懒创建、跨帧复用）。
    unsigned int gpu_driven_pbr_program_  = 0;
    unsigned int gpu_driven_pbr_pso_      = 0;
    bool         gpu_driven_pbr_failed_   = false;
    BufferHandle gpu_driven_perframe_ubo_{};
    BufferHandle gpu_driven_perscene_ubo_{};
    unsigned int white_texture_ = 0;                     ///< 1×1 默认白纹理（handle=0 回退）
};

}  // namespace render
}  // namespace dse

#endif  // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif  // DSE_WEBGPU_SHADER_MANAGER_H
