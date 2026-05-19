/**
 * @file vulkan_rhi_device.h
 * @brief Vulkan RHI 设备 — 实现 RhiDevice 抽象接口的 Vulkan 后端
 *
 * 架构对标 OpenGLRhiDevice，持有五个子系统并委托调用：
 * - VulkanContext：Instance/Device/Swapchain 生命周期
 * - VulkanResourceManager：GPU 资源创建/销毁/查询
 * - VulkanPipelineStateManager：VkPipeline/VkRenderPass 缓存与应用
 * - VulkanShaderManager：SPIR-V 模块编译与反射
 * - VulkanDrawExecutor：绘制命令执行
 */

#ifndef DSE_RENDER_VULKAN_RHI_DEVICE_H
#define DSE_RENDER_VULKAN_RHI_DEVICE_H

#include "engine/render/rhi/vulkan/vulkan_command_buffer.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace dse {
namespace render {

/**
 * @class VulkanRhiDevice
 * @brief RHI 的 Vulkan 实现 — 协调器，持有五个子系统并委托调用
 */
class VulkanRhiDevice final : public RhiDevice {
public:
    using RhiDevice::SetGlobalSpotShadowMap;
    using RhiDevice::SetGlobalSpotLightSpaceMatrix;

    VulkanRhiDevice() = default;
    ~VulkanRhiDevice() = default;

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

    /// 初始化 Vulkan 上下文（替代 OpenGL 的 glad 初始化）
    /// @param window_handle Win32 HWND
    /// @param width 窗口宽度
    /// @param height 窗口高度
    bool InitVulkan(void* window_handle, int width, int height, bool enable_validation = true);

    // --- Vulkan 特定扩展 ---
    void SetGlobalShadowMap(unsigned int index, unsigned int handle) override {
        draw_executor_.SetGlobalShadowMap(index, handle);
    }
    void SetGlobalSpotShadowMap(unsigned int index, unsigned int handle) override {
        draw_executor_.SetGlobalSpotShadowMap(index, handle);
    }
    void SetGlobalPointShadowMap(unsigned int index, unsigned int handle) override {
        draw_executor_.SetGlobalPointShadowMap(index, handle);
    }
    void SetGlobalLightSpaceMatrix(unsigned int index, const glm::mat4& mat) override {
        draw_executor_.SetGlobalLightSpaceMatrix(index, mat);
    }
    void SetGlobalCascadeSplit(unsigned int index, float split) override {
        draw_executor_.SetGlobalCascadeSplit(index, split);
    }
    void SetGlobalSpotLightSpaceMatrix(unsigned int index, const glm::mat4& mat) override {
        draw_executor_.SetGlobalSpotLightSpaceMatrix(index, mat);
    }
    void SetGlobalLightProbeSH(const glm::vec4 sh[9], bool enabled) override {
        draw_executor_.SetGlobalLightProbeSH(sh, enabled);
    }
    void SetGlobalGBufferTexture(unsigned int index, unsigned int texture_handle) override {
        draw_executor_.SetGlobalGBufferTexture(index, texture_handle);
    }
    void SetGBufferRenderingMode(bool enabled) override {
        draw_executor_.SetGBufferRenderingMode(enabled);
    }

    // --- SSBO（Clustered Forward+ 所需） ---
#pragma warning(push)
#pragma warning(disable: 4996)
    unsigned int CreateSSBO(size_t size, const void* data) override;
    void UpdateSSBO(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void BindSSBO(unsigned int handle, unsigned int binding_point) override {
        // 存储绑定状态，实际绑定在 DrawMeshBatch 的 descriptor set 分配中完成
        bound_ssbos_[binding_point] = handle;
    }
    void DeleteSSBO(unsigned int handle) override;

    // --- Compute Shader ---
    unsigned int CreateComputeShader(const std::string& source) override;
    void DeleteComputeShader(unsigned int handle) override;
    void DispatchCompute(unsigned int shader_handle, unsigned int groups_x, unsigned int groups_y, unsigned int groups_z) override;
    void ComputeMemoryBarrier() override;
    void BeginComputePass() override;
    void EndComputePass() override;
    void SetComputeTextureImage(unsigned int binding, unsigned int texture_handle, bool read_only) override;
    void SetComputeTextureImageMip(unsigned int binding, unsigned int texture_handle,
                                   int mip_level, bool read_only, bool r32f = false) override;
    void SetComputeTextureSampler(unsigned int unit, unsigned int texture_handle) override;
    bool SupportsCompute() const override { return true; }
    bool SupportsSSBOCompute() const override { return true; }

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
    void SetComputeUniformVec3(unsigned int shader, const char* name, float x, float y, float z) override;
    void SetComputeUniformIVec3(unsigned int shader, const char* name, int x, int y, int z) override;
    void SetComputeUniformVec4(unsigned int shader, const char* name, float x, float y, float z, float w) override;
    void SetComputeUniformMat4(unsigned int shader, const char* name, const float* data) override;
    void ReadSSBO(unsigned int handle, size_t offset, size_t size, void* dst) override;

    unsigned int CreateComputeShaderEx(
        const std::string& gl_src, const std::string& vk_src, const std::string& hlsl_src,
        uint32_t ssbo_count, uint32_t storage_image_count, uint32_t sampler_count,
        uint32_t push_constant_bytes) override;
    unsigned int CreateComputeWriteTexture2D(int width, int height) override;

    // --- Indirect Draw Buffer ---
    unsigned int CreateIndirectBuffer(size_t size, const void* data) override;
    void UpdateIndirectBuffer(unsigned int handle, size_t offset, size_t size, const void* data) override;
    void DeleteIndirectBuffer(unsigned int handle) override;
#pragma warning(pop)
    void MultiDrawIndexedIndirect(unsigned int indirect_buffer, int draw_count, size_t stride) override;
    bool SupportsIndirectDraw() const override { return true; }

    void SetActiveRenderCommandBuffer(VkCommandBuffer cmd) { active_render_cmd_ = cmd; }
    void ClearActiveRenderCommandBuffer() { active_render_cmd_ = VK_NULL_HANDLE; }

    bool NeedsTextureYFlip() const override { return true; }
    bool NeedsReadbackYFlip() const override { return false; }

    /// Vulkan: Y-flip (NDC Y-down) + Z remap ([-1,1] → [0,1])
    glm::mat4 GetProjectionCorrection() const override {
        // row-major construction: column 0, column 1, column 2, column 3
        return glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 0.5f, 0.0f,
            0.0f,  0.0f, 0.5f, 1.0f
        );
    }

    /// Shadow sampling: Y-flip only, NO Z remap.
    /// Shader will remap Z from [-1,1] to [0,1] uniformly.
    glm::mat4 GetShadowSampleCorrection() const override {
        return glm::mat4(
            1.0f,  0.0f, 0.0f, 0.0f,
            0.0f, -1.0f, 0.0f, 0.0f,
            0.0f,  0.0f, 1.0f, 0.0f,
            0.0f,  0.0f, 0.0f, 1.0f
        );
    }

    // --- 子系统访问器 ---
    VulkanContext& context() { return context_; }
    VulkanResourceManager& resource_mgr() { return resource_mgr_; }
    VulkanPipelineStateManager& state_mgr() { return state_mgr_; }
    VulkanShaderManager& shader_mgr() { return shader_mgr_; }
    VulkanDrawExecutor& draw_executor() { return draw_executor_; }

    /// 获取当前帧绑定的 SSBO 状态（binding_point → handle）
    const std::unordered_map<unsigned int, unsigned int>& bound_ssbos() const { return bound_ssbos_; }

private:
    void EnsureInitialized();

    /// 子系统实例
    VulkanContext context_;
    VulkanResourceManager resource_mgr_;
    VulkanPipelineStateManager state_mgr_;
    VulkanShaderManager shader_mgr_;
    VulkanDrawExecutor draw_executor_;

    /// 通过 CreateShaderProgram 外部创建的着色器句柄
    std::unordered_set<unsigned int> external_shader_programs_;

    RenderStats last_frame_stats_;
    RenderStats current_frame_stats_;

    /// 本帧待提交的命令缓冲列表
    std::vector<VkCommandBuffer> pending_command_buffers_;

    /// 当前活跃的渲染命令缓冲（由 VulkanCommandBuffer::BeginRenderPass 设置）
    VkCommandBuffer active_render_cmd_ = VK_NULL_HANDLE;

    /// 当前帧绑定的 SSBO 状态 (binding_point → handle)
    std::unordered_map<unsigned int, unsigned int> bound_ssbos_;

    /// Compute pass 批量录制状态
    VkCommandBuffer compute_cmd_buffer_ = VK_NULL_HANDLE;
    bool in_compute_pass_ = false;

    /// Compute push constant 缓冲（用于 SetComputeUniform* 系列）
    std::vector<uint8_t> compute_push_constants_;

    /// Pending compute image 绑定 (binding → texture_handle)
    struct ComputeImageBinding {
        unsigned int texture_handle = 0;
        bool read_only = true;
        int mip_level = -1;  ///< -1 表示全 mip
        bool r32f = false;
    };
    std::unordered_map<unsigned int, ComputeImageBinding> pending_compute_images_;
    std::unordered_map<unsigned int, unsigned int> pending_compute_samplers_; ///< unit → tex handle

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_RHI_DEVICE_H
