/**
 * @file rhi_device.h
 * @brief 渲染硬件接口(RHI)抽象层 — 纯虚基类 RhiDevice + CommandBuffer
 *
 * 本文件仅包含后端无关的抽象接口。
 * 具体后端实现位于各自头文件：
 * - gl_command_buffer.h + gl_rhi_device.h    (OpenGL)
 * - vulkan/vulkan_rhi_device.h (Vulkan)
 * - dx11/dx11_rhi_device.h     (DX11)
 */

#ifndef DSE_RHI_DEVICE_H
#define DSE_RHI_DEVICE_H

#include <vector>
#include <functional>
#include <unordered_map>
#include <glm/glm.hpp>
#include <memory>
#include <string>
#include "engine/render/rhi/rhi_types.h"
#include "engine/render/rhi/postprocess_common.h"
#include "engine/render/rhi/rhi_compute.h"
#include "engine/render/rhi/rhi_storage_buffer.h"
#include "engine/render/rhi/rhi_gpu_driven.h"
#include "engine/render/rhi/rhi_gpu_buffer.h"
#include "engine/render/rhi/rhi_gpu_timer.h"
#include "engine/render/rhi/draw_executor_common.h"

namespace dse {
namespace render {

/**
 * @class CommandBuffer
 * @brief 命令缓冲抽象类，负责收集和记录一帧内所有的渲染指令
 */
class CommandBuffer {
public:
    virtual ~CommandBuffer() = default;
    virtual void BeginRenderPass(const RenderPassDesc& render_pass) = 0;
    virtual void EndRenderPass() = 0;
    virtual void SetPipelineState(unsigned int pipeline_state_handle) = 0;
    virtual void SetCamera(const glm::mat4& view, const glm::mat4& projection) = 0;
    virtual void DrawMeshBatch(const std::vector<MeshDrawItem>& items) = 0;
    virtual void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) = 0;
    virtual void ClearColor(const glm::vec4& color) = 0;
    virtual void SetGlobalMat4(const std::string& name, const glm::mat4& value) = 0;
    virtual void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) = 0;
    virtual void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) = 0;
    virtual void DrawPostProcess(PostProcessRequest request) = 0;
    virtual void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;
    virtual void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) = 0;

    /// 设置视口区域（用于 shadow atlas 等 viewport-based 渲染）
    virtual void SetViewport(int x, int y, int width, int height) = 0;

    /// 清除当前 viewport/scissor 区域的深度缓冲（用于 atlas 逐区域清除）
    virtual void ClearDepth(float depth = 1.0f) { (void)depth; }

    /// 诊断用：直接 blit RT 到 swapchain，绕过 shader pipeline
    virtual void BlitToScreen(unsigned int source_rt) { (void)source_rt; }

    /// 阴影贴图绑定命令（Pass 中调用，直接委托到 RhiDevice）
    virtual void BindGlobalShadowMap(unsigned int index, unsigned int texture_handle) = 0;
    virtual void BindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) = 0;
    virtual void BindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) = 0;

    // --- 通用绘制原语 (A1) ---
    // 高层渲染器（如 SkyboxRenderer）用这组后端无关的原语组合出绘制，
    // 取代把具体效果（DrawSkybox 等）做成 RHI 虚函数的做法。
    // 默认空实现：尚未实现该组原语的后端/Mock 仍可编译。

    /// 绑定着色器程序（取代各效果隐式绑定自己的 shader）
    virtual void BindShaderProgram(unsigned int program_handle) { (void)program_handle; }
    /// 绑定顶点缓冲 + 顶点布局（float 属性）。布局随 VB 一起提供，后端据此建立输入布局。
    virtual void BindVertexBuffer(unsigned int buffer_handle, uint32_t stride,
                                  const std::vector<VertexAttr>& attrs) {
        (void)buffer_handle; (void)stride; (void)attrs;
    }
    /// 绑定 cubemap 纹理到指定 slot
    virtual void BindTextureCube(unsigned int slot, unsigned int cubemap_handle) {
        (void)slot; (void)cubemap_handle;
    }
    /// 设置 push-constant 风格的 mat4（GL→uniform / Vulkan→push constant / DX11→CB）
    virtual void PushConstantsMat4(const glm::mat4& value) { (void)value; }
    /// 非索引绘制
    virtual void Draw(uint32_t vertex_count, uint32_t first_vertex = 0) {
        (void)vertex_count; (void)first_vertex;
    }

    // --- 通用绘制原语 (B0): 索引 / 2D 纹理 / UBO / 索引绘制 ---
    // 这组原语由 mesh/sprite 类消费者倒推（见 RHI_PRIMITIVE_CONTRACT.md §4）。
    // 默认空实现，未实现的后端/Mock 仍可编译。

    /// 绑定索引缓冲（供 DrawIndexed 使用）
    virtual void BindIndexBuffer(unsigned int buffer_handle, IndexType type) {
        (void)buffer_handle; (void)type;
    }
    /// 绑定纹理到指定 slot（按维度区分 2D / cube / 2D 数组）
    virtual void BindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim) {
        (void)slot; (void)texture_handle; (void)dim;
    }
    /// 绑定 uniform/constant buffer 到指定 slot（offset/size=0 表示整个 buffer）
    virtual void BindUniformBuffer(uint32_t slot, unsigned int buffer_handle,
                                   uint32_t offset = 0, uint32_t size = 0) {
        (void)slot; (void)buffer_handle; (void)offset; (void)size;
    }
    /// 索引绘制
    virtual void DrawIndexed(uint32_t index_count, uint32_t first_index = 0,
                             int32_t base_vertex = 0) {
        (void)index_count; (void)first_index; (void)base_vertex;
    }
};

/**
 * @class RhiDevice
 * @brief 渲染硬件接口基类，提供创建 GPU 资源（纹理、缓冲、着色器）及命令缓冲的统一抽象
 *
 * 阴影/光源全局状态接口：所有后端（OpenGL、Vulkan）均实现相同的阴影贴图与光源矩阵绑定，
 * 消除上层代码对具体后端的 dynamic_cast 依赖。
 */
/**
 * @brief 渲染设备运行时信息（实际所选 GPU/适配器名 + 是否软件渲染）。
 *        供性能基准等场景标注后端实际跑在硬件还是软渲，避免误读软渲数据。
 */
struct RenderDeviceInfo {
    std::string adapter_name = "unknown";
    bool is_software = false;
};

class RhiDevice : public IRhiCompute, public IRhiStorageBuffer, public IRhiGpuDriven, public IRhiGpuTimer {
public:
    virtual ~RhiDevice() = default;

    void SetInitKeepAlive(std::function<void()> cb) { init_keep_alive_ = std::move(cb); }

    /// 返回实际所选适配器名 + 是否软件渲染。默认 unknown/false，各后端覆写。
    virtual RenderDeviceInfo GetDeviceInfo() const { return {}; }

    /// 在窗口创建后初始化设备（D3D11/Vulkan 需要 HWND；OpenGL 默认已就绪）
    virtual bool InitDevice(void* window_handle, int width, int height) { (void)window_handle; (void)width; (void)height; return true; }

    /// 窗口大小改变时通知后端（Vulkan 需重建 Swapchain；GL/DX11 可重载按需处理）
    virtual void OnWindowResized(int width, int height) { (void)width; (void)height; }

    virtual void Shutdown() = 0;
    virtual void WaitIdle() {}
    virtual void BeginFrame() = 0;
    virtual unsigned int CreateRenderTarget(const RenderTargetDesc& desc) = 0;
    virtual void DeleteRenderTarget(unsigned int render_target_handle) { (void)render_target_handle; }
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const = 0;
    virtual unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const {
        (void)index; return GetRenderTargetColorTexture(render_target_handle);
    }
    virtual unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const = 0;
    virtual std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const = 0;
    virtual RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const = 0;
    virtual unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) = 0;
    /// 带采样描述(过滤 + 环绕)的 2D 纹理创建。默认实现退化为旧的 filter-only 行为
    /// (环绕 = Repeat)，后端可覆盖以支持 ClampToEdge / 点采样等。
    virtual unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                         const TextureSamplerDesc& sampler) {
        return CreateTexture2D(width, height, rgba8_data, sampler.filter == TextureFilter::Linear);
    }
    virtual unsigned int CreateCompressedTexture2D(CompressedTextureFormat format,
                                                   const std::vector<CompressedMipLevel>& mips,
                                                   bool linear_filter) { (void)format; (void)mips; (void)linear_filter; return 0; }
    virtual unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) = 0;
    virtual unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) = 0;
    virtual void DeleteTexture(unsigned int texture_handle) = 0;
    virtual unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) = 0;
    virtual void DeleteShaderProgram(unsigned int program_handle) = 0;
    virtual unsigned int CreatePipelineState(const PipelineStateDesc& desc) = 0;

    // --- 内建资源（供高层渲染器用通用原语绘制，A1）---
    // 着色器在各后端是预编译的（GL=GLSL / Vulkan=SPIR-V / DX11=DXBC），无法由后端无关层创建，
    // 故由各后端懒初始化并通过下列访问器暴露句柄。默认返回 0（未实现该资源的后端）。

    /// 内建天空盒着色器程序句柄（懒初始化）
    virtual unsigned int GetSkyboxShaderProgram() { return 0; }
    /// 内建天空盒立方体顶点缓冲句柄（36 顶点，vec3 pos，懒初始化）
    virtual unsigned int GetSkyboxCubeVertexBuffer() { return 0; }
    /// 内建 2D sprite 着色器程序句柄（pos\@0/color\@1/uv\@2 + PerFrame UBO + u_texture，懒初始化）
    /// 供 B0 SpriteRenderer 用新通用原语做活体验证。
    virtual unsigned int GetSprite2DShaderProgram() { return 0; }

    virtual unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) = 0;
    virtual void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) = 0;
    virtual void DeleteBuffer(unsigned int handle) = 0;

    // --- 资源状态转换（RenderGraph 自动屏障） ---

    /// 渲染目标状态转换，由 RenderGraph::Execute 在 Pass 之间自动调用。
    /// 默认实现：从 UnorderedAccess 离开时插入 ComputeMemoryBarrier。
    /// 后端可覆写以执行更精确的屏障（如 Vulkan VkImageMemoryBarrier）。
    virtual void TransitionRenderTarget(unsigned int rt_handle,
                                         ResourceState from, ResourceState to) {
        (void)rt_handle;
        if (from == ResourceState::UnorderedAccess && to != ResourceState::UnorderedAccess) {
            ComputeMemoryBarrier();
        }
    }

    // --- 统一 GPU Buffer API（Phase 2）---
    // 默认实现转发到旧 API，后端可覆写以获取完整 usage 信息。
    // 旧 CreateBuffer/CreateSSBO/CreateIndirectBuffer 已标记 deprecated。
    // 以下默认实现有意调用旧 API 做路由，后端可覆写以直接处理。

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4996)  // deprecated
#endif

    virtual BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) {
        BufferHandle h;
        if (has(desc.usage, GpuBufferUsage::kStorage)) {
            h = BufferHandle{CreateSSBO(desc.size, initial_data)};
        } else if (has(desc.usage, GpuBufferUsage::kIndirect)) {
            h = BufferHandle{CreateIndirectBuffer(desc.size, initial_data)};
        } else {
            bool is_index = has(desc.usage, GpuBufferUsage::kIndex);
            h = BufferHandle{CreateBuffer(desc.size, initial_data, desc.is_dynamic, is_index)};
        }
        if (h) gpu_buffer_usage_map_[h.raw()] = desc.usage;
        return h;
    }

    virtual void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) {
        auto usage = GetBufferUsage_(handle);
        if (has(usage, GpuBufferUsage::kStorage)) {
            UpdateSSBO(handle.raw(), offset, size, data);
        } else if (has(usage, GpuBufferUsage::kIndirect)) {
            UpdateIndirectBuffer(handle.raw(), offset, size, data);
        } else {
            bool is_index = has(usage, GpuBufferUsage::kIndex);
            UpdateBuffer(handle.raw(), offset, size, data, is_index);
        }
    }

    virtual void DeleteGpuBuffer(BufferHandle handle) {
        auto usage = GetBufferUsage_(handle);
        if (has(usage, GpuBufferUsage::kStorage)) {
            DeleteSSBO(handle.raw());
        } else if (has(usage, GpuBufferUsage::kIndirect)) {
            DeleteIndirectBuffer(handle.raw());
        } else {
            DeleteBuffer(handle.raw());
        }
        gpu_buffer_usage_map_.erase(handle.raw());
    }

    virtual void BindGpuBuffer(BufferHandle handle, uint32_t binding_point) {
        BindSSBO(handle.raw(), binding_point);
    }

    /// 绑定 GPU Buffer 到指定绑定点（writable=true 时 DX11 绑定为 UAV，GL/VK 忽略此参数）
    virtual void BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) {
        (void)writable;
        BindGpuBuffer(handle, binding_point);
    }

    virtual void ReadGpuBuffer(BufferHandle handle, size_t offset, size_t size, void* dst) {
        ReadSSBO(handle.raw(), offset, size, dst);
    }

    /// 异步 readback: 发起 GPU→staging 拷贝（本帧不读取），返回上一帧数据是否可用
    /// 默认实现: 同步读取并立即返回 true（OpenGL readback 足够快）
    virtual bool BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) {
        async_readback_buf_.resize(size);
        ReadGpuBuffer(handle, offset, size, async_readback_buf_.data());
        return true;
    }

    /// 获取上一次 BeginGpuReadback 的结果（仅当 BeginGpuReadback 返回 true 时有效）
    virtual const void* GetLastReadbackResult(size_t* out_size = nullptr) const {
        if (out_size) *out_size = async_readback_buf_.size();
        return async_readback_buf_.data();
    }

private:
    std::vector<uint8_t> async_readback_buf_;
public:

#ifdef _MSC_VER
#pragma warning(pop)
#endif
    virtual VertexArrayHandle CreateVertexArray() = 0;
    virtual void DeleteVertexArray(VertexArrayHandle handle) = 0;
    virtual std::shared_ptr<CommandBuffer> CreateCommandBuffer() = 0;
    virtual void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) = 0;
    virtual void EndFrame() = 0;

    /// 将当前帧提交到显示器（交换链 Present）
    /// 在 EndFrame 之后、主循环 render 计时之外调用，避免 Present 延迟污染 avg_render_ms
    /// 默认空实现（OpenGL 由 GLFW SwapBuffers 处理）
    virtual void PresentFrame() {}

    virtual const RenderStats& LastFrameStats() const = 0;

    /// 编辑器用帧统计概要，从 LastFrameStats() 派生
    struct RhiFrameStats {
        int draw_calls      = 0;
        int triangle_count  = 0;
        int sprite_count    = 0;
        int texture_binds   = 0;  ///< 映射自 material_switches
        int shader_switches = 0;  ///< 映射自 material_batch_count
    };

    virtual RhiFrameStats GetFrameStats() const {
        const auto& s = LastFrameStats();
        RhiFrameStats fs;
        fs.draw_calls      = s.draw_calls;
        fs.triangle_count  = s.triangle_count;
        fs.sprite_count    = s.sprite_count;
        fs.texture_binds   = s.material_switches;
        fs.shader_switches = s.material_batch_count;
        return fs;
    }

    /// GPU→CPU 读回是否低成本（OpenGL: 是; DX11: 否，同步 pipeline flush）
    /// 返回 false 时跳过 GPU Driven / Hi-Z readback 以避免帧卡顿
    virtual bool SupportsEfficientReadback() const { return true; }

    /// 在 EndFrame 之后补写 GPU Driven 剔除统计（因 readback 在 EndFrame 后发生）
    virtual void PatchLastFrameGPUCulledCount(int culled) { (void)culled; }

    // --- 阴影/光源全局状态接口（所有后端统一，直接操作共享 DrawExecutorGlobalState） ---
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) { global_render_state_.SetShadowMap(index, handle); }
    void SetGlobalSpotShadowMap(unsigned int handle) { global_render_state_.SetSpotShadowMap(0, handle); }
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) { global_render_state_.SetSpotShadowMap(index, handle); }
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) { global_render_state_.SetPointShadowMap(index, handle); }
    void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) { global_render_state_.SetLightSpaceMatrix(index, mat); }
    void SetGlobalCascadeSplit(unsigned int index, float split) { global_render_state_.SetCascadeSplit(index, split); }
    void SetGlobalShadowAtlasRegion(unsigned int index, const glm::vec4& region) { global_render_state_.SetShadowAtlasRegion(index, region); }
    void SetGlobalSpotLightSpaceMatrix(const glm::mat4& mat) { global_render_state_.SetSpotLightSpaceMatrix(0, mat); }
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) { global_render_state_.SetSpotLightSpaceMatrix(index, mat); }
    void SetGlobalLightProbeSH(const glm::vec4 sh[9], bool enabled) { global_render_state_.SetLightProbeSH(sh, enabled); }

    // --- DDGI 全局状态 ---
    void SetGlobalDDGI(bool enabled, unsigned int irradiance_atlas,
                       const glm::vec3& grid_origin, const glm::vec3& grid_spacing,
                       const glm::ivec3& grid_resolution, int irradiance_texels,
                       float gi_intensity, float normal_bias) {
        global_render_state_.SetDDGI(enabled, irradiance_atlas, grid_origin, grid_spacing,
                                     grid_resolution, irradiance_texels, gi_intensity, normal_bias);
    }

    // --- 全局湿度 ---
    void SetGlobalWetness(float wetness) { global_render_state_.global_wetness = wetness; }

    // --- 植被风参数 ---
    void SetGlobalFoliageWind(const glm::vec4& wind) { global_render_state_.foliage_wind = wind; }
    void SetGlobalFoliagePush(const glm::vec4& push) { global_render_state_.foliage_push = push; }

    // --- GBuffer / Deferred 管线状态 ---
    void SetGlobalGBufferTexture(unsigned int index, unsigned int texture_handle) { global_render_state_.SetGBufferTexture(index, texture_handle); }
    void SetGBufferRenderingMode(bool enabled) { global_render_state_.gbuffer_rendering_mode = enabled; }

    /// 全局渲染状态访问器（供 DrawExecutor 等内部组件使用）
    DrawExecutorGlobalState& GetGlobalRenderState() { return global_render_state_; }
    const DrawExecutorGlobalState& GetGlobalRenderState() const { return global_render_state_; }

    /// OpenGL textures need Y-flip on load (bottom-left origin); D3D11/Vulkan don't
    virtual bool NeedsTextureYFlip() const { return true; }
    /// OpenGL readback is bottom-up and needs flip; D3D11/Vulkan readback is top-down
    virtual bool NeedsReadbackYFlip() const { return true; }

    /// Clip-space correction matrix to convert from OpenGL NDC convention
    /// (Y-up, Z∈[-1,1]) to the target API convention.
    /// OpenGL: identity. Vulkan: Y-flip + Z remap. DX11: Z remap only.
    virtual glm::mat4 GetProjectionCorrection() const { return glm::mat4(1.0f); }

    /// Shadow-sampling correction: same as GetProjectionCorrection() but WITHOUT
    /// Z remap, so the shader can consistently remap Z from [-1,1] to [0,1].
    /// OpenGL: identity (same).  Vulkan: Y-flip only.  DX11: identity.
    virtual glm::mat4 GetShadowSampleCorrection() const { return glm::mat4(1.0f); }

    /// 编辑器场景视图模式: 线框/Unlit/Overdraw 等渲染状态控制
    virtual void SetWireframeMode(bool enable) { (void)enable; }
    virtual void SetForceUnlit(bool enable) { (void)enable; }
    virtual void SetOverdrawMode(bool enable) { (void)enable; }

    // --- 扩展能力查询（委托到继承的接口，此处仅保留 PatchLastFrameGPUCulledCount）---

protected:
    std::function<void()> init_keep_alive_;
    void KeepAlive() { if (init_keep_alive_) init_keep_alive_(); }

    GpuBufferUsage GetBufferUsage_(BufferHandle handle) const {
        auto it = gpu_buffer_usage_map_.find(handle.raw());
        return it != gpu_buffer_usage_map_.end() ? it->second : GpuBufferUsage::kVertex;
    }

    std::unordered_map<unsigned int, GpuBufferUsage> gpu_buffer_usage_map_;

    /// 共享全局渲染状态（阴影/光源/GBuffer/DDGI/LightProbe），三端通过引用访问
    DrawExecutorGlobalState global_render_state_;
};

} // namespace render
} // namespace dse

// 兼容性 using：大量上层代码仍以全局命名空间形式引用这两个类
using dse::render::CommandBuffer;
using dse::render::RhiDevice;

#endif
