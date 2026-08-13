/**
 * @file gl_rhi_device.h
 * @brief OpenGL RHI 后端实现
 *
 * 从 rhi_device.h 拆分：rhi_device.h 只保留纯虚基类 RhiDevice + CommandBuffer，
 * 本文件包含 OpenGLRhiDevice 及其 GL 子系统依赖。
 */

#ifndef DSE_GL_RHI_DEVICE_H
#define DSE_GL_RHI_DEVICE_H

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/opengl/gl_resource_manager.h"
#include "engine/render/rhi/opengl/gl_pipeline_state_manager.h"
#include "engine/render/rhi/opengl/gl_shader_manager.h"
#include "engine/render/rhi/opengl/gl_draw_executor.h"
#include "engine/render/rhi/opengl/ubo_manager.h"
#include "engine/render/rhi/opengl/gl_gpu_timer.h"
#include <unordered_set>
#include <unordered_map>
#include <memory>
#include <string>
#include <vector>

namespace dse {
namespace render {

/**
 * @class OpenGLRhiDevice
 * @brief RHI 的 OpenGL 实现 - 协调器，持有五个子系统并委托调用
 *
 * 子系统架构：
 * - GLResourceManager：GPU 资源创建/销毁/查询（RenderTarget/Texture/Buffer/VAO/PipelineState）
 * - GLPipelineStateManager：渲染状态缓存与应用（blend/depth/culling）
 * - GLShaderManager：着色器编译/链接/Uniform location 缓存
 * - GLDrawExecutor：绘制命令执行（2D 精灵/3D 网格/天空盒/后处理/粒子）
 * - UBOManager：Uniform Buffer Object 生命周期与数据更新（PerFrame/PerScene/PerMaterial）
 */
class OpenGLRhiDevice final : public RhiDevice {
public:
    OpenGLRhiDevice();
    ~OpenGLRhiDevice() override;

    RenderDeviceInfo GetDeviceInfo() const override;

    void Shutdown() override;
    void BeginFrame() override;
    RenderTargetHandle CreateRenderTarget(const RenderTargetDesc& desc) override;
    void DeleteRenderTarget(RenderTargetHandle render_target_handle) override;
    RhiBackend GetBackend() const override { return RhiBackend::OpenGL; }
    TextureHandle GetRenderTargetColorTexture(RenderTargetHandle render_target_handle) const override;
    TextureHandle GetRenderTargetColorTexture(RenderTargetHandle render_target_handle, int index) const override;
    TextureHandle GetRenderTargetDepthTexture(RenderTargetHandle render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(RenderTargetHandle render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(RenderTargetHandle render_target_handle) const override;
    RenderTargetDepthReadback ReadRenderTargetDepthFloatWithSize(RenderTargetHandle render_target_handle) const override;
    void ImmediateDraw(const ImmediateDrawDesc& desc) override;
    void BlitRenderTarget(RenderTargetHandle src_rt, RenderTargetHandle dst_rt) override;
    BufferHandle CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;
    void UpdateBuffer(BufferHandle handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(BufferHandle handle) override;
    VertexArrayHandle CreateVertexArray() override;
    void DeleteVertexArray(VertexArrayHandle handle) override;
    TextureHandle CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    TextureHandle CreateTexture2D(int width, int height, const unsigned char* rgba8_data,
                                 const TextureSamplerDesc& sampler) override;
    TextureHandle CreateCompressedTexture2D(CompressedTextureFormat format,
                                           const std::vector<CompressedMipLevel>& mips,
                                           bool linear_filter) override;
    TextureHandle CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
    TextureHandle CreateTextureCubeWithMips(const std::vector<CubeMipLevel>& mips,
                                           bool linear_filter) override;
    TextureHandle CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) override;
    void DeleteTexture(TextureHandle texture_handle) override;
    void UpdateTextureSubRegion(TextureHandle texture_handle, int x, int y,
                                int width, int height, const unsigned char* rgba8_data) override;
    ShaderHandle CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(ShaderHandle program_handle) override;
    PipelineHandle CreatePipelineState(const PipelineStateDesc& desc) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    const RenderStats& LastFrameStats() const override;

    // --- RenderGraph 自动屏障（GL: glTextureBarrier / glMemoryBarrier）---
    void TransitionRenderTarget(RenderTargetHandle rt_handle,
                                 ResourceState from, ResourceState to) override;

    void PatchLastFrameGPUCulledCount(int culled) override {
        draw_executor_.MutableLastFrameStats().gpu_culled_count = culled;
    }

    // --- SSBO（Clustered Forward+ 所需） ---
    // 旧 API 已 deprecated，但后端实现仍需覆写供基类路由调用
#pragma warning(push)
#pragma warning(disable: 4996)
    BufferHandle CreateSSBO(size_t size, const void* data) override;
    void UpdateSSBO(BufferHandle handle, size_t offset, size_t size, const void* data) override;
    void BindSSBO(BufferHandle handle, unsigned int binding_point) override;
    void DeleteSSBO(BufferHandle handle) override;
    bool SupportsSSBO() const override { return supports_ssbo_; }
    int GetMaxColorAttachments() const override { return max_color_attachments_; }

    // --- Compute Shader（GL 4.3+）---
    ShaderHandle CreateComputeShader(const std::string& source) override;
    void DeleteComputeShader(ShaderHandle handle) override;
    void DispatchCompute(ShaderHandle shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void ComputeMemoryBarrier() override;
    void SetComputeTextureImage(unsigned int binding, TextureHandle texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, TextureHandle texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, TextureHandle texture_handle) override;
    bool SupportsCompute() const override { return supports_ssbo_; }
    bool SupportsSSBOCompute() const override { return supports_ssbo_; }
    TextureHandle CreateComputeWriteTexture2D(int width, int height) override;

    // --- Hi-Z Occlusion Culling ---
    TextureHandle CreateHiZTexture(int width, int height) override;
    void DeleteHiZTexture(TextureHandle handle) override;
    int GetHiZMipCount(TextureHandle handle) const override;
    TextureHandle GetHiZGpuTexture(TextureHandle handle) const override;

    // --- Compute Uniform ---
    void SetComputeUniformInt(ShaderHandle shader, const char* name, int value) override;
    void SetComputeUniformFloat(ShaderHandle shader, const char* name, float value) override;
    void SetComputeUniformVec2i(ShaderHandle shader, const char* name, int x, int y) override;
    void SetComputeUniformVec2f(ShaderHandle shader, const char* name, float x, float y) override;
    void SetComputeUniformVec3(ShaderHandle shader, const char* name, float x, float y, float z) override;
    void SetComputeUniformIVec3(ShaderHandle shader, const char* name, int x, int y, int z) override;
    void SetComputeUniformVec4(ShaderHandle shader, const char* name, float x, float y, float z, float w) override;
    void SetComputeUniformMat4(ShaderHandle shader, const char* name, const float* data) override;

    // --- SSBO 读回 ---
    void ReadSSBO(BufferHandle handle, size_t offset, size_t size, void* dst) override;

    // --- Indirect Draw Buffer ---
    BufferHandle CreateIndirectBuffer(size_t size, const void* data) override;
    void UpdateIndirectBuffer(BufferHandle handle, size_t offset, size_t size, const void* data) override;
    void DeleteIndirectBuffer(BufferHandle handle) override;
#pragma warning(pop)
    void MultiDrawIndexedIndirect(BufferHandle indirect_buffer, int draw_count, size_t stride, size_t byte_offset = 0) override;
    bool SupportsIndirectDraw() const override { return supports_ssbo_; }

    // --- Mega Buffer (GPU Driven) ---
    VertexArrayHandle CreateMegaVAO(size_t vbo_size_bytes, size_t ibo_size_bytes,
                               BufferHandle& out_vbo, BufferHandle& out_ibo) override;
    void UpdateMegaVBO(BufferHandle vbo, size_t offset, size_t size, const void* data) override;
    void UpdateMegaIBO(BufferHandle ibo, size_t offset, size_t size, const void* data) override;
    void DeleteMegaVAO(VertexArrayHandle vao, BufferHandle vbo, BufferHandle ibo) override;
    void BindMegaVAO(VertexArrayHandle vao) override;
    void UnbindVAO() override;

    // --- Static Mesh VAO ---
    VertexArrayHandle CreateStaticMeshVAO(
        const void* vertex_data, size_t vertex_bytes,
        const std::vector<const void*>& ebo_datas,
        const std::vector<size_t>& ebo_sizes,
        BufferHandle& out_vbo,
        std::vector<BufferHandle>& out_ebos) override;
    void DeleteStaticMeshVAO(VertexArrayHandle vao, BufferHandle vbo,
                              const std::vector<BufferHandle>& ebos) override;
    void BindVAOWithEBO(VertexArrayHandle vao, BufferHandle ebo) override;
    bool HasGPUDrivenPBRShader() const override;
    void SetupGPUDrivenPBRShader(const glm::mat4& view, const glm::mat4& proj,
                                  const glm::vec3& camera_pos,
                                  const glm::vec3& light_dir, const glm::vec3& light_color,
                                  float light_intensity, float ambient_intensity,
                                  float shadow_strength = 0.0f) override;
    void SetupGPUDrivenShadowShader(const glm::mat4& light_view, const glm::mat4& light_proj) override;
    void BindGPUDrivenTextures(TextureHandle albedo, TextureHandle normal,
                                TextureHandle metallic_roughness,
                                TextureHandle emissive, TextureHandle occlusion) override;

    // --- 编辑器场景视图模式 ---
    void SetWireframeMode(bool enable) override;
    void SetForceUnlit(bool enable) override;
    void SetOverdrawMode(bool enable) override;

    // --- 内部方法（供 OpenGLCommandBuffer 直接调用，委托到子系统） ---
    void RealBeginRenderPass(const RenderPassDesc& render_pass);
    void RealEndRenderPass();
    void RealClearColor(const glm::vec4& color);

    // --- 通用绘制原语 (A1) ---
    void RealBindPipeline(GraphicsPipelineHandle graphics_pipeline_handle);
    void RealBindVertexBuffer(uint32_t slot, BufferHandle buffer_handle, uint32_t stride,
                              const std::vector<VertexAttr>& attrs,
                              VertexInputRate rate = VertexInputRate::PerVertex);
    void RealPushConstants(ShaderStage stage, uint32_t offset, const void* data, uint32_t size);
    void RealDraw(uint32_t vertex_count, uint32_t first_vertex);

    // --- 通用绘制原语 (B0) ---
    void RealBindIndexBuffer(BufferHandle buffer_handle, IndexType type);
    void RealBindTexture(uint32_t slot, TextureHandle texture_handle, TextureDim dim);
    void RealBindUniformBuffer(uint32_t slot, BufferHandle buffer_handle, uint32_t offset, uint32_t size);
    void RealBindStorageBuffer(uint32_t slot, BufferHandle buffer_handle, uint32_t offset, uint32_t size);
    void RealDrawIndexed(uint32_t index_count, uint32_t first_index, int32_t base_vertex);

    // --- 通用绘制原语 (B2b 前置): 实例化索引绘制 ---
    void RealDrawIndexedInstanced(uint32_t index_count, uint32_t instance_count,
                                  uint32_t first_index, int32_t base_vertex,
                                  uint32_t first_instance);

    // --- 通用绘制原语 (B2b-5): GPU-driven 间接索引绘制 ---
    void RealDrawIndexedIndirect(BufferHandle indirect_buffer, uint32_t byte_offset);

    // --- 内建资源访问器 ---
    ShaderHandle GetBuiltinProgram(BuiltinProgram program) override;
    ShaderHandle GetGenPPShaderProgram(const std::string& effect_name) override;
    BufferHandle GetSkyboxCubeVertexBuffer() override;

    // --- 子系统访问器 ---
    GLResourceManager& resource_mgr() { return resource_mgr_; }
    GLPipelineStateManager& state_mgr() { return state_mgr_; }
    GLShaderManager& shader_mgr() { return shader_mgr_; }
    GLDrawExecutor& draw_executor() { return draw_executor_; }
    UBOManager& ubo_mgr() { return ubo_mgr_; }

private:
    void EnsureInitialized();
    void LogResourceLedger() const;

    /// 子系统实例
    GLResourceManager resource_mgr_;
    GLPipelineStateManager state_mgr_;
    GLShaderManager shader_mgr_;
    GLDrawExecutor draw_executor_{global_render_state_};
    UBOManager ubo_mgr_;

    /// 通过 CreateShaderProgram 外部创建的着色器句柄，需在 Shutdown 中统一清理
    std::unordered_set<ShaderHandle> external_shader_programs_;

    /// 内建天空盒立方体顶点缓冲（懒初始化，A1 通用原语用）
    BufferHandle skybox_cube_vbo_;

    /// 通过 CreateComputeShader 创建的 compute 程序句柄
    std::unordered_set<ShaderHandle> compute_programs_;

    /// Hi-Z 纹理信息（pimpl 隐藏实现，避免 WINDOWS_EXPORT_ALL_SYMBOLS 模板泄漏）
    struct HiZImpl;
    std::unique_ptr<HiZImpl> hiz_impl_;

    /// GPU Timestamp Query 子系统
    GLGpuTimer gpu_timer_;

    /// Indirect draw buffer 管理
    std::unordered_map<BufferHandle, unsigned int> indirect_buffers_; ///< handle → GL buffer ID
    unsigned int next_indirect_handle_ = 600000;

    bool initialized_ = false;
    bool supports_ssbo_ = true;  ///< GL 4.3+ 支持 SSBO；GL 3.3 fallback 为 false
    int max_color_attachments_ = 8;  ///< GL_MAX_COLOR_ATTACHMENTS，Init 时探测（MRT 上限）
    int max_samples_ = 1;            ///< GL_MAX_SAMPLES，Init 时探测（MSAA 采样上限，1=不支持）

    // --- Compute push 常量 → DsePushCS UBO 降级支撑（契约 §8.2）---
    // 编译器把 compute 的 layout(push_constant) 降级为 std140 块 DsePushCS（无 binding）。
    // SetComputeUniform* 按调用序累积到 staging（与 DX11/Vulkan 同的 16B 对齐定位方案），
    // DispatchCompute 时反射块 + 绑保留 binding + backing UBO + 整块上传。
    static constexpr unsigned int kComputePushUboBinding = 13;  ///< 避开真 UBO/SSBO 低位 binding
    struct ComputePushUbo {
        unsigned int ubo = 0;       ///< backing UBO（0=该程序无 DsePushCS 块）
        int size = 0;               ///< 块字节大小（GL_UNIFORM_BLOCK_DATA_SIZE）
        bool initialized = false;
    };
    std::unordered_map<ShaderHandle, ComputePushUbo> compute_push_ubos_;
    std::vector<uint8_t> compute_uniform_staging_;             ///< CPU 暂存（定位累积，整块上传）
    std::unordered_map<std::string, size_t> compute_uniform_name_to_offset_;
    size_t compute_uniform_next_offset_ = 0;
    size_t GetOrCreateComputeUniformOffset(const char* name, size_t data_size);
    ComputePushUbo& EnsureComputePushUbo(ShaderHandle program);

public:
    // --- IRhiGpuTimer 接口 ---
    bool SupportsGpuTimer() const override { return gpu_timer_.SupportsGpuTimer(); }
    GpuTimerId GetOrCreateGpuTimer(const std::string& name) override { return gpu_timer_.GetOrCreateGpuTimer(name); }
    void BeginGpuTimer(GpuTimerId id) override { gpu_timer_.BeginGpuTimer(id); }
    void EndGpuTimer(GpuTimerId id) override { gpu_timer_.EndGpuTimer(id); }
    float GetGpuTimerResultMs(GpuTimerId id) const override { return gpu_timer_.GetGpuTimerResultMs(id); }
    void ResetGpuTimers() override { gpu_timer_.ResetGpuTimers(); }
    void ResolveGpuTimers() override { gpu_timer_.ResolveGpuTimers(); }
    std::vector<GpuTimerEntry> GetAllGpuTimerResults() const override { return gpu_timer_.GetAllGpuTimerResults(); }
};

} // namespace render
} // namespace dse

#endif
