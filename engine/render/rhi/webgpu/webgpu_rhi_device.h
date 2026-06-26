/**
 * @file webgpu_rhi_device.h
 * @brief WebGPU RHI 后端（orchestrator）。
 *
 * 目标：把 Web 从 WebGL2(GLES3.0) 前向尽力版提升到与桌面 Vulkan/D3D11 对等
 * （Compute + SSBO + GPU-driven 全链路）。本后端与 OpenGL 后端并存：WebGPU 可用
 * 时走 parity 路径，否则由上层回退到阶段 A 的 WebGL2 路径（见 rhi_factory）。
 *
 * 仅在 Emscripten + DSE_ENABLE_WEBGPU 下编入（见顶层 CMakeLists 与 rhi_factory 守卫）。
 * 工具链：Emscripten 内置 WebGPU 绑定（-sUSE_WEBGPU=1，<webgpu/webgpu.h>）。
 * 设备由 JS 侧（shell.html）预创建，C++ 经 emscripten_webgpu_get_device() 取得。
 *
 * 本类已按 manager 对称拆分退化为 orchestrator：持 ctx/res/pso/shader/exec 五个 manager，
 * 自留设备生命周期编排（Init/Shutdown/BeginFrame/EndFrame/PresentFrame）与少量元信息查询，
 * 其余所有 RHI 虚函数为一行转发到对应 manager（零功能/零调用方 API 改动）。详见
 * docs/architecture/WEBGPU_MANAGER_SPLIT_PLAN.md。
 */

#ifndef DSE_WEBGPU_RHI_DEVICE_H
#define DSE_WEBGPU_RHI_DEVICE_H

#if defined(__EMSCRIPTEN__) && defined(DSE_ENABLE_WEBGPU)

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/webgpu/webgpu_context.h"
#include "engine/render/rhi/webgpu/webgpu_resource_manager.h"
#include "engine/render/rhi/webgpu/webgpu_pipeline_state_manager.h"
#include "engine/render/rhi/webgpu/webgpu_shader_manager.h"
#include "engine/render/rhi/webgpu/webgpu_draw_executor.h"

#include <webgpu/webgpu.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dse {
namespace render {

#ifdef DSE_WEBGPU_SELFTEST
class WebGpuSelfTestHarness;
#endif

/**
 * @class WebGPURhiDevice
 * @brief RHI 的 WebGPU 实现（orchestrator）。
 *
 * 持 ctx/res/pso/shader/exec 五个 manager（构造序 ctx→res→pso→shader→exec），各 manager
 * 在 ctx AcquireDevice 后经 DeviceAcquired 回调同步同名稳定句柄 device_/queue_。
 */
class WebGPURhiDevice final : public RhiDevice {
public:
    WebGPURhiDevice();
    ~WebGPURhiDevice() override;

    // --- 设备生命周期 ---
    RenderDeviceInfo GetDeviceInfo() const override;
    /// MRT 上限：AcquireDevice 经 wgpuDeviceGetLimits 读取适配器实际
    /// maxColorAttachments 填充（默认 8）。供能力声明式裁剪 requires_mrt 的 pass 精确判定。
    int GetMaxColorAttachments() const override { return ctx_->max_color_attachments(); }
    bool InitDevice(void* window_handle, int width, int height) override;
    void OnWindowResized(int width, int height) override;
    void Shutdown() override;
    void WaitIdle() override;
    void BeginFrame() override;
    void EndFrame() override;
    void PresentFrame() override;

    // --- 渲染目标 ---
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    void DeleteRenderTarget(unsigned int render_target_handle) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;

    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const override;

    // --- 纹理 ---
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                 const TextureSamplerDesc& sampler) override;
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
    unsigned int CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips, bool linear_filter) override;
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) override;
    void DeleteTexture(unsigned int texture_handle) override;

    // --- 着色器 / 管线状态 ---
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(unsigned int program_handle) override;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;

    // --- 内建资源（手写 WGSL，经通用原语上屏）---
    unsigned int GetBuiltinProgram(BuiltinProgram program) override;
    unsigned int GetGenPPShaderProgram(const std::string& effect_name) override;
    unsigned int GetSkyboxCubeVertexBuffer() override;

    // --- 缓冲 ---
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;

    // --- VAO / 命令缓冲 ---
    VertexArrayHandle CreateVertexArray() override;
    void DeleteVertexArray(VertexArrayHandle handle) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;

    const RenderStats& LastFrameStats() const override;

    // --- 约定矫正（WebGPU 与 D3D12/Metal 同：Z∈[0,1]，纹理 top-left 原点）---
    bool NeedsTextureYFlip() const override { return false; }
    bool NeedsReadbackYFlip() const override { return false; }
    glm::mat4 GetProjectionCorrection() const override;
    glm::mat4 GetShadowSampleCorrection() const override;

    // ============================================================
    // 设备级命令录制 API（WebGPUCommandBuffer 逐调用转发至此 → 再转发 exec_）
    // ============================================================
    void CmdBeginRenderPass(const RenderPassDesc& desc);
    void CmdEndRenderPass();
    void CmdClearColor(const glm::vec4& color);
    void CmdSetViewport(int x, int y, int width, int height);

    void CmdBindGlobalShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdBindGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle);
    void CmdBindGlobalPointShadowMap(unsigned int index, unsigned int texture_handle);

    void CmdBindPipeline(unsigned int graphics_pipeline_handle);
    void CmdBindVertexBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t stride,
                             const std::vector<VertexAttr>& attrs, VertexInputRate rate);
    void CmdBindIndexBuffer(unsigned int buffer_handle, IndexType type);
    void CmdBindTexture(uint32_t slot, unsigned int texture_handle, TextureDim dim);
    void CmdBindUniformBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size);
    void CmdBindStorageBuffer(uint32_t slot, unsigned int buffer_handle, uint32_t offset, uint32_t size);
    void CmdPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size);

    void CmdDraw(uint32_t vertex_count, uint32_t first_vertex);
    void CmdDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);
    void CmdDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                 uint32_t first_index, int32_t base_vertex, uint32_t first_instance);
    void CmdDrawIndexedIndirect(unsigned int indirect_buffer, uint32_t byte_offset);
    void CmdDispatchComputePass(const ComputeDispatch& dispatch);

    // ============================================================
    // Compute 能力：SupportsCompute()/SupportsSSBOCompute() 均为 true。
    // ============================================================
    bool SupportsCompute() const override { return true; }
    bool SupportsSSBOCompute() const override { return true; }

    unsigned int CreateComputeShader(const std::string& source) override;
    unsigned int CreateComputeShaderEx(
        const std::string& gl_src, const std::string& vk_src, const std::string& hlsl_src,
        uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count,
        uint32_t push_constant_bytes, const std::string& wgsl_src = "") override;
    void DeleteComputeShader(unsigned int handle) override;
    void DispatchCompute(unsigned int shader_handle,
                         unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void BeginComputePass() override;
    void EndComputePass() override;
    unsigned int CreateComputeWriteTexture2D(int width, int height) override;
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    void SetComputeUniformInt(unsigned int shader, const char* name, int value) override;
    void SetComputeUniformFloat(unsigned int shader, const char* name, float value) override;
    void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) override;
    void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) override;
    void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) override;
    void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) override;
    void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) override;
    void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) override;

    BufferHandle CreateGpuBuffer(const GpuBufferDesc& desc, const void* initial_data) override;
    void UpdateGpuBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) override;
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point) override;
    void BindGpuBuffer(BufferHandle handle, uint32_t binding_point, bool writable) override;
    void DeleteGpuBuffer(BufferHandle handle) override;

    bool BeginGpuReadback(BufferHandle handle, size_t offset, size_t size) override;
    const void* GetLastReadbackResult(size_t* out_size = nullptr) const override;

    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride,
                                  size_t byte_offset = 0) override;

    bool SupportsIndirectDraw() const override { return true; }

    VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                                    BufferHandle& out_vbo, BufferHandle& out_ibo) override;
    void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) override;
    void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) override;
    void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) override;
    void BindMegaVAO(VertexArrayHandle vao) override;
    void UnbindVAO() override;

    bool HasGPUDrivenPBRShader() const override;
    void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                 const glm::vec3& camera_pos,
                                 const glm::vec3& light_dir, const glm::vec3& light_color,
                                 float light_intensity, float ambient_intensity,
                                 float shadow_strength = 0.0f) override;
    void BindGPUDrivenTextures(unsigned int albedo, unsigned int normal,
                               unsigned int metallic_roughness,
                               unsigned int emissive, unsigned int occlusion) override;

    unsigned int CreateHiZTexture(int width, int height) override;
    void DeleteHiZTexture(unsigned int handle) override;
    int GetHiZMipCount(unsigned int handle) const override;
    unsigned int GetHiZGpuTexture(unsigned int handle) const override;

private:
    // --- manager（构造序 ctx→res→pso→shader→exec；声明序须与之一致以保正确析构逆序）---
    std::unique_ptr<WebGPUContext> ctx_;
    WebGPUResourceManager      res_;
    WebGPUPipelineStateManager pso_;
    WebGPUShaderManager        shader_;
    WebGPUDrawExecutor         exec_;

    RenderStats last_frame_stats_{};

    // --- 自检 harness（诊断代码，外置 + 编译期门控 DSE_WEBGPU_SELFTEST，默认关闭）---
#ifdef DSE_WEBGPU_SELFTEST
    friend class WebGpuSelfTestHarness;
    WebGpuSelfTestHarness* selftest_ = nullptr;

    // harness friend 只读访问器（字段已移入各 manager；harness 机械 field→accessor 迁移用）。
    WGPUDevice device() const { return ctx_->device(); }
    WGPUQueue queue() const { return ctx_->queue(); }
    WGPUCommandEncoder frame_encoder() const { return ctx_->frame_encoder(); }
    int width() const { return ctx_->width(); }
    int height() const { return ctx_->height(); }
    WGPURenderPassEncoder cur_pass() const { return exec_.cur_pass(); }
    WGPUComputePassEncoder cur_compute_pass() const { return exec_.cur_compute_pass(); }
    // harness 调用的私有方法（转发到对应 manager）。
    void ResetDrawState() { exec_.ResetDrawState(); }
    WGPUShaderModule CompileWGSL(const std::string& code, const char* label) { return shader_.CompileWGSL(code, label); }
    unsigned int CreateTextureImpl(WGPUTextureDimension dim, WGPUTextureViewDimension view_dim,
                                   uint32_t width, uint32_t height, uint32_t depth_or_layers,
                                   WGPUTextureFormat format, WGPUTextureUsageFlags usage,
                                   uint32_t mip_levels, int msaa_samples,
                                   const std::vector<const unsigned char*>& layer_data,
                                   const TextureSamplerDesc& sampler) {
        return res_.CreateTextureImpl(dim, view_dim, width, height, depth_or_layers, format, usage,
                                      mip_levels, msaa_samples, layer_data, sampler);
    }
    // harness 句柄表只读查询（条目类型已移入 webgpu_common.h，转发到 res_）。
    const BufferEntry* FindBuffer(unsigned int handle) const { return res_.FindBuffer(handle); }
    const TextureEntry* FindTexture(unsigned int handle) const { return res_.FindTexture(handle); }
    const RenderTargetEntry* FindRenderTarget(unsigned int handle) const { return res_.FindRenderTarget(handle); }
    bool EnsureGpuDrivenPBRShader() { return shader_.EnsureGpuDrivenPBRShader(); }
#endif
};

} // namespace render
} // namespace dse

#endif // __EMSCRIPTEN__ && DSE_ENABLE_WEBGPU
#endif // DSE_WEBGPU_RHI_DEVICE_H
