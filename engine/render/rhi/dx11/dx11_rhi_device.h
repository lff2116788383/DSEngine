/**
 * @file dx11_rhi_device.h
 * @brief D3D11 RHI 设备 — 实现 RhiDevice 抽象接口的 D3D11 后端
 *
 * 架构对标 OpenGLRhiDevice / VulkanRhiDevice，持有子系统并委托调用：
 * - DX11Context：Device/DeviceContext/SwapChain 生命周期
 * - DX11ResourceManager：纹理/Buffer/RenderTarget 生命周期管理
 * - DX11ShaderManager：HLSL 着色器编译与管理
 * - DX11PipelineStateManager：BlendState/DepthStencilState/RasterizerState
 * - DX11DrawExecutor：绘制命令执行
 */

#ifndef DSE_RENDER_DX11_RHI_DEVICE_H
#define DSE_RENDER_DX11_RHI_DEVICE_H

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/dx11/dx11_context.h"
#include "engine/render/rhi/dx11/dx11_resource_manager.h"
#include "engine/render/rhi/dx11/dx11_shader_manager.h"
#include "engine/render/rhi/dx11/dx11_pipeline_state_manager.h"
#include "engine/render/rhi/dx11/dx11_draw_executor.h"

#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace dse {
namespace render {

/**
 * @class DX11CommandBuffer
 * @brief D3D11 命令缓冲实现，录制渲染命令并提交到 DX11RhiDevice
 *
 * Phase 1: stub 实现 — 所有方法为空操作
 */
class DX11CommandBuffer final : public CommandBuffer {
public:
    void BeginRenderPass(const RenderPassDesc& render_pass) override;
    void EndRenderPass() override;
    void SetPipelineState(unsigned int pipeline_state_handle) override;
    void SetCamera(const glm::mat4& view, const glm::mat4& projection) override;
    void DrawBatch(const std::vector<DrawBatchItem>& items) override;
    void DrawMeshBatch(const std::vector<MeshDrawItem>& items) override;
    void DrawSpriteBatch(const std::vector<SpriteDrawItem>& items) override;
    void ClearColor(const glm::vec4& color) override;
    void SetGlobalMat4(const std::string& name, const glm::mat4& value) override;
    void SetGlobalMat4Array(const std::string& name, const std::vector<glm::mat4>& values) override;
    void SetGlobalFloatArray(const std::string& name, const std::vector<float>& values) override;
    void DrawSkybox(unsigned int cubemap_texture_handle) override;
    void DrawPostProcess(unsigned int source_texture, const std::string& effect_name, const std::vector<float>& params) override;
    void DrawParticles3D(const std::vector<Particle3DDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void DrawHairStrands(const std::vector<HairDrawItem>& items, const glm::mat4& view, const glm::mat4& projection) override;
    void DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) override;
    void DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override;
    void DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override;

    /// 重置命令缓冲状态
    void Reset();

    /// 设置所属设备（由 DX11RhiDevice::CreateCommandBuffer 注入）
    void SetDevice(class DX11RhiDevice* device) { device_ = device; }

    // --- 全局 uniform 访问器（供后续 DrawExecutor 读取） ---
    const std::unordered_map<std::string, glm::mat4>& pending_mat4() const { return pending_mat4_; }
    const std::unordered_map<std::string, std::vector<glm::mat4>>& pending_mat4_array() const { return pending_mat4_array_; }
    const std::unordered_map<std::string, std::vector<float>>& pending_float_array() const { return pending_float_array_; }
    void ClearPendingUniforms() {
        pending_mat4_.clear();
        pending_mat4_array_.clear();
        pending_float_array_.clear();
    }

private:
    DX11RhiDevice* device_ = nullptr;
    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 projection_ = glm::mat4(1.0f);

    /// 全局 uniform 暂存（SetGlobalMat4 等），绘制时由 DrawExecutor 消费
    std::unordered_map<std::string, glm::mat4> pending_mat4_;
    std::unordered_map<std::string, std::vector<glm::mat4>> pending_mat4_array_;
    std::unordered_map<std::string, std::vector<float>> pending_float_array_;
};

/**
 * @class DX11RhiDevice
 * @brief RHI 的 D3D11 实现 — 协调器，持有所有子系统并委托调用
 */
class DX11RhiDevice final : public RhiDevice {
public:
    using RhiDevice::SetGlobalSpotShadowMap;
    using RhiDevice::SetGlobalSpotLightSpaceMatrix;

    DX11RhiDevice() = default;
    ~DX11RhiDevice() = default;

    // --- RhiDevice 接口 ---
    bool InitDevice(void* window_handle, int width, int height) override;
    void Shutdown() override;
    void BeginFrame() override;
    unsigned int CreateRenderTarget(const RenderTargetDesc& desc) override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle) const override;
    unsigned int GetRenderTargetColorTexture(unsigned int render_target_handle, int index) const override;
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateCompressedTexture2D(CompressedTextureFormat format,
                                           const std::vector<CompressedMipLevel>& mips,
                                           bool linear_filter) override;
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
    unsigned int CreateTexture3D(int width, int height, int depth, const unsigned char* rgba8_data, bool linear_filter) override;
    void DeleteTexture(unsigned int texture_handle) override;
    unsigned int CreateShaderProgram(const std::string& vert_src, const std::string& frag_src) override;
    void DeleteShaderProgram(unsigned int program_handle) override;
    unsigned int CreatePipelineState(const PipelineStateDesc& desc) override;
    unsigned int CreateBuffer(size_t size, const void* data, bool is_dynamic, bool is_index) override;
    void UpdateBuffer(unsigned int handle, size_t offset, size_t size, const void* data, bool is_index) override;
    void DeleteBuffer(unsigned int handle) override;
    unsigned int CreateVertexArray() override;
    void DeleteVertexArray(unsigned int handle) override;
    std::shared_ptr<CommandBuffer> CreateCommandBuffer() override;
    void Submit(std::shared_ptr<CommandBuffer> cmd_buffer) override;
    void EndFrame() override;
    const RenderStats& LastFrameStats() const override;

    // --- 阴影/光源全局状态 ---
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) override;
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) override;
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) override;
    void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) override;
    void SetGlobalCascadeSplit(unsigned int index, float split) override;
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) override;
    void SetGlobalLightProbeSH(const glm::vec4 sh[9], bool enabled) override;
    void SetGlobalGBufferTexture(unsigned int index, unsigned int texture_handle) override;
    void SetGBufferRenderingMode(bool enabled) override;

    // --- SSBO（Clustered Forward+ 所需） ---
    unsigned int CreateSSBO(size_t size, const void* data) override;
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void BindSSBO(unsigned int handle, unsigned int binding_point) override;
    void DeleteSSBO(unsigned int handle) override;

    // --- Compute Shader ---
    unsigned int CreateComputeShader(const std::string& source) override;
    void DeleteComputeShader(unsigned int handle) override;
    void DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void ComputeMemoryBarrier() override;
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    bool SupportsCompute() const override { return true; }

    // --- Hi-Z Occlusion Culling ---
    unsigned int CreateHiZTexture(int width, int height) override;
    void DeleteHiZTexture(unsigned int handle) override;
    int GetHiZMipCount(unsigned int handle) const override;
    unsigned int GetHiZGpuTexture(unsigned int handle) const override;

    // --- Compute Uniform ---
    void SetComputeUniformInt(unsigned int shader, const char* name, int value) override;
    void SetComputeUniformFloat(unsigned int shader, const char* name, float value) override;
    void SetComputeUniformVec2i(unsigned int shader, const char* name, int x, int y) override;
    void SetComputeUniformVec2f(unsigned int shader, const char* name, float x, float y) override;
    void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) override;
    void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) override;
    void ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) override;

    // --- Indirect Draw Buffer (桩) ---
    unsigned int CreateIndirectBuffer(size_t size, const void* data) override { (void)size; (void)data; return 0; }
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) override { (void)handle; (void)offset; (void)size; (void)data; }
    void DeleteIndirectBuffer(unsigned int handle) override { (void)handle; }
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride) override { (void)indirect_buffer; (void)draw_count; (void)stride; }
    bool SupportsIndirectDraw() const override { return false; }

    bool NeedsTextureYFlip() const override { return true; }
    bool NeedsReadbackYFlip() const override { return false; }

    /// DX11: Z remap only ([-1,1] → [0,1]), Y stays up
    glm::mat4 GetProjectionCorrection() const override {
        return glm::mat4(
            1.0f, 0.0f, 0.0f, 0.0f,
            0.0f, 1.0f, 0.0f, 0.0f,
            0.0f, 0.0f, 0.5f, 0.0f,
            0.0f, 0.0f, 0.5f, 1.0f
        );
    }

    /// Shadow sampling: identity (no Z remap).
    /// Shader will remap Z from [-1,1] to [0,1] uniformly.
    glm::mat4 GetShadowSampleCorrection() const override {
        return glm::mat4(1.0f);
    }

    /// 初始化 D3D11 上下文
    bool InitD3D11(void* window_handle, int width, int height, bool enable_debug = false, bool force_sdr = false);

    // --- 子系统访问器 ---
    DX11Context& context() { return context_; }
    DX11ResourceManager& resource_mgr() { return resource_mgr_; }
    DX11ShaderManager& shader_mgr() { return shader_mgr_; }
    DX11PipelineStateManager& state_mgr() { return state_mgr_; }
    DX11DrawExecutor& draw_executor() { return draw_executor_; }

private:
    /// 子系统实例
    DX11Context context_;
    DX11ResourceManager resource_mgr_;
    DX11ShaderManager shader_mgr_;
    DX11PipelineStateManager state_mgr_;
    DX11DrawExecutor draw_executor_;

    /// 通过 CreateShaderProgram 外部创建的着色器句柄
    std::unordered_set<unsigned int> external_shader_programs_;

    /// Compute Shader 绑定的 SSBO 追踪（binding_point → handle）
    std::unordered_map<unsigned int, unsigned int> bound_ssbos_;

    RenderStats last_frame_stats_;
    RenderStats current_frame_stats_;

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_DX11_RHI_DEVICE_H
