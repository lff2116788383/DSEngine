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

#include "engine/render/rhi/rhi_device.h"
#include "engine/render/rhi/vulkan/vulkan_context.h"
#include "engine/render/rhi/vulkan/vulkan_resource_manager.h"
#include "engine/render/rhi/vulkan/vulkan_pipeline_state_manager.h"
#include "engine/render/rhi/vulkan/vulkan_shader_manager.h"
#include "engine/render/rhi/vulkan/vulkan_draw_executor.h"

#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>

namespace dse {
namespace render {

/**
 * @class VulkanCommandBuffer
 * @brief Vulkan 命令缓冲实现，录制渲染命令并提交到 VulkanRhiDevice
 *
 * 与 OpenGLCommandBuffer 的 variant 式命令列表不同，
 * Vulkan 版本直接使用 VkCommandBuffer 录制命令，
 * Submit 时通过 vkEndCommandBuffer + vkQueueSubmit 提交。
 */
class VulkanCommandBuffer final : public CommandBuffer {
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
    void DeferSetGlobalShadowMap(unsigned int index, unsigned int texture_handle) override;
    void DeferSetGlobalSpotShadowMap(unsigned int index, unsigned int texture_handle) override;
    void DeferSetGlobalPointShadowMap(unsigned int index, unsigned int texture_handle) override;

    /// 获取底层 VkCommandBuffer
    VkCommandBuffer GetVkCommandBuffer() const { return vk_command_buffer_; }
    void SetVkCommandBuffer(VkCommandBuffer cmd) { vk_command_buffer_ = cmd; }

    /// 重置命令缓冲状态
    void Reset();

    /// 设置所属设备（由 VulkanRhiDevice::CreateCommandBuffer 注入）
    void SetDevice(class VulkanRhiDevice* device) { device_ = device; }

    // --- 全局 uniform 访问器（供 VulkanDrawExecutor 读取） ---
    const std::unordered_map<std::string, glm::mat4>& pending_mat4() const { return pending_mat4_; }
    const std::unordered_map<std::string, std::vector<glm::mat4>>& pending_mat4_array() const { return pending_mat4_array_; }
    const std::unordered_map<std::string, std::vector<float>>& pending_float_array() const { return pending_float_array_; }
    void ClearPendingUniforms() {
        pending_mat4_.clear();
        pending_mat4_array_.clear();
        pending_float_array_.clear();
    }

private:
    VkCommandBuffer vk_command_buffer_ = VK_NULL_HANDLE;
    VulkanRhiDevice* device_ = nullptr;
    glm::mat4 view_ = glm::mat4(1.0f);
    glm::mat4 projection_ = glm::mat4(1.0f);

    /// 全局 uniform 暂存（SetGlobalMat4 等），绘制时由 DrawExecutor 消费
    std::unordered_map<std::string, glm::mat4> pending_mat4_;
    std::unordered_map<std::string, std::vector<glm::mat4>> pending_mat4_array_;
    std::unordered_map<std::string, std::vector<float>> pending_float_array_;
};

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
    unsigned int GetRenderTargetDepthTexture(unsigned int render_target_handle) const override;
    std::vector<unsigned char> ReadRenderTargetColorRgba8(unsigned int render_target_handle) const override;
    RenderTargetReadback ReadRenderTargetColorRgba8WithSize(unsigned int render_target_handle) const override;
    unsigned int CreateTexture2D(int width, int height, const unsigned char* rgba8_data, bool linear_filter) override;
    unsigned int CreateTextureCube(int width, int height, const unsigned char* const rgba8_faces[6], bool linear_filter) override;
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

    bool NeedsTextureYFlip() const override { return false; }
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

    // --- 子系统访问器 ---
    VulkanContext& context() { return context_; }
    VulkanResourceManager& resource_mgr() { return resource_mgr_; }
    VulkanPipelineStateManager& state_mgr() { return state_mgr_; }
    VulkanShaderManager& shader_mgr() { return shader_mgr_; }
    VulkanDrawExecutor& draw_executor() { return draw_executor_; }

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

    bool initialized_ = false;
};

} // namespace render
} // namespace dse

#endif // DSE_RENDER_VULKAN_RHI_DEVICE_H
